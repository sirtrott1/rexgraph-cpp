#pragma once

#include "rfc/learning/relational_task_outcome_codec.hpp"
#include "rfc/inference/relational_finite_source_codec.hpp"

namespace rfc::learning {
struct RelationalAssignmentDeclaration {
    std::uint64_t key{}, revision{}, origin{}, parameter_carrier{};
    std::vector<std::uint64_t> parameters;
};
struct RelationalAssignmentCandidate {
    std::uint64_t key{};
    std::vector<std::uint64_t> assignment;
    RelationalWorkerBindingDeclaration binding;
    PreparedAvailableRelationalEvidence evidence;
    numeric::RationalVector shift;
    numeric::ExactMatrix injection;
};
struct RelationalAssignmentInput {
    std::uint64_t candidate{};
    inference::AvailableRelationalFieldResult input;
};
struct RelationalAssignmentObservation {
    std::uint64_t key{}, origin{};
    RelationalWorkerBindingDeclaration binding;
    inference::AvailableRelationalFieldResult measured;
    std::vector<RelationalAssignmentInput> inputs;
};
struct RelationalAssignmentCoordinates {
    numeric::RationalVector shift;
    numeric::ExactMatrix map;
};
struct RelationalAssignmentSolve {
    std::uint64_t observation{};
    AvailableRelationalAffineResult response;
    RelationalTaskPostconditionCertificate certificate;
    RelationalAssignmentCoordinates child;
};
struct RelationalAssignmentBranch {
    RelationalAssignmentCoordinates coordinates;
    std::vector<std::shared_ptr<const RelationalAssignmentSolve>> solves;
    bool rejected{};
};
enum class RelationalAssignmentStatus : std::uint8_t { Inconsistent, Unresolved, Identified };
struct RelationalAssignmentOccurrenceMap {
    numeric::ExactMatrix particular;
    std::vector<numeric::ExactMatrix> directions;
};
namespace assignment_evidence_detail {
using namespace task_outcome_detail;
inline constexpr std::array<std::byte, 8> root_magic{
    std::byte{'R'},std::byte{'F'},std::byte{'C'},std::byte{'A'},std::byte{'S'},std::byte{1},std::byte{0},std::byte{0}};
inline constexpr auto measured_magic=[] { auto m=root_magic; m[4]=std::byte{'M'}; return m; }();
struct CandidateRecord {
    std::uint64_t key{};
    std::vector<std::uint64_t> assignment;
    RelationalWorkerBindingDeclaration binding;
    V shift;
    Matrix injection;
    Bytes evidence;
};
inline void declaration(inference::field_program_codec_detail::Writer& w,const RelationalAssignmentDeclaration& d) {
    w.u64(d.key);w.u64(d.revision);w.u64(d.parameter_carrier);w.u64(d.parameters.size());
    for(const auto p:d.parameters)w.u64(p);
}
inline void candidate(inference::field_program_codec_detail::Writer& w,const CandidateRecord& c) {
    w.u64(c.key);w.u64(c.assignment.size());for(const auto x:c.assignment)w.u64(x);
    binding(w,c.binding);w.u64(c.binding.origin);
    relational_affine_codec_detail::vector(w,c.shift);storage::temporal_codec_detail::matrix(w,c.injection);
    inference::field_program_codec_detail::blob(w,c.evidence);
}
inline CandidateRecord record(const RelationalAssignmentCandidate& c) {
    return {c.key,c.assignment,c.binding,c.shift,c.injection,encode_available_relational_evidence(c.evidence)};
}
inline Bytes root_payload(const RelationalAssignmentDeclaration& d,std::span<const std::byte> finite,
    const std::vector<CandidateRecord>& records,numeric::ExactActionLimits limits) {
    using namespace inference::field_program_codec_detail;
    auto w=envelope(root_magic);declaration(w,d);w.u64(limits.dimension);w.u64(limits.entries);w.u64(limits.steps);w.u64(limits.integer_bits);
    blob(w,finite);w.u64(records.size());for(const auto& c:records)candidate(w,c);return finish(std::move(w));
}
inline Bytes measurement_payload(std::uint64_t key,const RelationalWorkerBindingDeclaration& b,
    std::span<const std::byte> measured,const std::vector<std::pair<std::uint64_t,Bytes>>& inputs) {
    using namespace inference::field_program_codec_detail;
    auto w=envelope(measured_magic);w.u64(key);binding(w,b);w.u64(b.origin);blob(w,measured);w.u64(inputs.size());
    for(const auto& [id,bytes]:inputs){w.u64(id);blob(w,bytes);}return finish(std::move(w));
}
inline Origin admit_binding(const RelationalWorkerBindingDeclaration& b,const inference::OwnedRelationalFieldProgram& p,
    const Context& authority) {
    require(b.key&&b.revision&&b.worker&&b.machine&&b.operation,"actual worker/machine/operation binding identities required");
    return authority.require(b.origin,Role::Maps,binding_payload(b,inference::encode_relational_field_program(p)));
}
inline void one_input(const inference::OwnedRelationalFieldProgram& p) {
    require(std::count_if(p.declaration().nodes.begin(),p.declaration().nodes.end(),[](const auto& n){
        return n.kind==inference::FieldProgramNodeKind::Input;})==1,"assignment v1 requires one complete original input port");
}
inline void same_input(const inference::AvailableRelationalFieldResult& a,const inference::AvailableRelationalFieldResult& b,Guard& guard) {
    const auto x=a.result(),y=b.result();
    require(x.inputs().size()==1&&y.inputs().size()==1&&a.input_origins().size()==1&&b.input_origins().size()==1,
        "assignment v1 requires one complete original input use");
    const auto& u=x.inputs().front();const auto& v=y.inputs().front();
    require(a.input_origins().front().asset==b.input_origins().front().asset&&u.use()==v.use()&&
        inference::field_interface_detail::equal(u.source_field(),v.source_field()),
        "assignment observations need the same actual input asset origin, use and whole source field");
    const auto ua=u.accession(),va=v.accession();const auto& d=ua.declaration();const auto& e=va.declaration();
    require(d.source_grade==e.source_grade&&d.source_stalks.size()==e.source_stalks.size(),"assignment original input carrier differs");
    for(std::size_t i=0;i<d.source_stalks.size();++i){guard.step();const auto& s=d.source_stalks[i];const auto& t=e.source_stalks[i];
        require(s.cell==t.cell&&s.basis==t.basis&&s.directions==t.directions&&s.metric==t.metric,
            "assignment input needs actual ordered source bases and metrics");}
}
inline RelationalAffineField restrict_field(const RelationalAffineField& f,const RelationalAssignmentCoordinates& c,Guard& guard) {
    return {tensor(sum(f.particular.values().dense(),numeric::checked_apply(f.directions,c.shift,guard),guard),guard),
        numeric::checked_multiply(f.directions,c.map,guard)};
}
inline RelationalAssignmentCoordinates compose(const RelationalAssignmentCoordinates& outer,
    const RelationalAssignmentCoordinates& inner,Guard& guard) {
    return {sum(outer.shift,numeric::checked_apply(outer.map,inner.shift,guard),guard),
        numeric::checked_multiply(outer.map,inner.map,guard)};
}
inline void same_frame(const RelationalWorkerBindingDeclaration& a,const RelationalWorkerBindingDeclaration& b) {
    require(a.view.basis==b.view.basis&&a.view.directions==b.view.directions,
        "assignment outputs need one explicit common observation basis");
}
} // namespace assignment_evidence_detail
inline std::vector<std::byte> relational_assignment_root_payload(const RelationalAssignmentDeclaration& d,
    const inference::OwnedRelationalFiniteSource& finite,const std::vector<RelationalAssignmentCandidate>& candidates,
    numeric::ExactActionLimits limits={}) {
    std::vector<assignment_evidence_detail::CandidateRecord> records;
    for(const auto& c:candidates)records.push_back(assignment_evidence_detail::record(c));
    return assignment_evidence_detail::root_payload(d,inference::encode_relational_finite_source(finite),records,limits);
}
inline std::vector<std::byte> relational_assignment_measurement_payload(const RelationalAssignmentObservation& o) {
    std::vector<std::pair<std::uint64_t,assignment_evidence_detail::Bytes>> inputs;
    for(const auto& i:o.inputs)inputs.emplace_back(i.candidate,inference::encode_available_relational_result(i.input));
    return assignment_evidence_detail::measurement_payload(o.key,o.binding,
        inference::encode_available_relational_result(o.measured),inputs);
}
class RelationalAssignmentResponse;

// Each finite assignment retains its own compatible exact solve family. Evidence
// intersects that family; incompatible assignments keep their refutation rather
// than being replaced by a score or a selected parameter setting.
class PreparedRelationalAssignmentEvidence {
    struct Fixed {
        inference::OwnedRelationalFiniteSource finite;
        RelationalAssignmentDeclaration declaration;
        std::vector<RelationalAssignmentCandidate> candidates;
        inference::RelationalFiniteSections sections;
        std::vector<std::vector<std::uint64_t>> remainder;
        assignment_evidence_detail::Origin origin;
        std::vector<assignment_evidence_detail::Origin> bindings;
        numeric::ExactActionLimits limits;
    };
    struct State {
        std::shared_ptr<const Fixed> fixed;
        std::uint64_t revision{};
        std::vector<RelationalAssignmentBranch> branches;
        std::vector<RelationalAssignmentObservation> observations;
        std::vector<assignment_evidence_detail::Origin> origins,bindings;
        std::shared_ptr<const State> parent;
        std::size_t retained{};
    };
public:
    PreparedRelationalAssignmentEvidence(RelationalAssignmentDeclaration declaration,
        inference::OwnedRelationalFiniteSource finite,std::vector<RelationalAssignmentCandidate> candidates,
        const inference::RelationalAvailabilityContext& context,numeric::ExactActionLimits limits={},std::function<void()> check={}) {
        using namespace assignment_evidence_detail;
        const auto authority=context;Guard guard(limits,check);
        require(declaration.key&&declaration.revision&&declaration.parameter_carrier&&!candidates.empty(),"identified nonempty assignment family required");
        std::vector<Origin> bindings;
        for(const auto& c:candidates){c.evidence.admit(authority,check);bindings.push_back(admit_binding(c.binding,c.evidence.program().program(),authority));}
        const auto origin=authority.require(declaration.origin,Role::Evidence,relational_assignment_root_payload(declaration,finite,candidates,limits));
        std::set<std::uint64_t> axes,keys;const auto n=declaration.parameters.size();guard.dimension(n);
        for(const auto k:declaration.parameters){guard.step();require(k&&axes.insert(k).second,"assignment parameter identities must be distinct");}
        auto sections=finite.sections(limits,check);std::set<std::vector<std::uint64_t>> assignments;
        for(std::size_t i=0;i<sections.size();++i)assignments.insert(sections.assignment(i));
        std::vector<RelationalAssignmentBranch> branches;
        std::size_t retained=add(declaration.parameters.size(),sections.size());
        for(const auto& c:candidates){
            guard.step();require(c.key&&keys.insert(c.key).second&&assignments.erase(c.assignment)==1,"candidate needs a unique actual compatible finite assignment");
            same_limits(c.evidence.evidence().limits(),limits);one_input(c.evidence.program().program());
            const auto native=c.evidence.evidence();const auto free=native.free_parameters().size();
            guard.vector(c.shift);guard.matrix(c.injection);
            require(free==n&&c.shift.size()==n&&c.injection.rows()==n&&c.injection.cols()==n,
                "assignment v1 requires complete common/root free coordinate carriers");
            require(numeric::PreparedExactFrames(c.injection,guard).rank()==n,"branch injection must retain every original free direction");
            same_frame(c.binding,candidates.front().binding);
            require(c.binding.view.basis&&!c.binding.view.directions.empty()&&!c.binding.view.physical,
                "assignment candidate needs an explicit faithful rational field view");
            retained=add(retained,add(add(c.shift.size(),c.injection.nonzeros()),add(n,n)));guard.entries(retained);
            branches.push_back({{V(n),Matrix::identity(n)},{},false});
        }
        std::vector<std::vector<std::uint64_t>> remainder(assignments.begin(),assignments.end());
        auto fixed=std::make_shared<const Fixed>(Fixed{std::move(finite),declaration,std::move(candidates),std::move(sections),
            std::move(remainder),origin,std::move(bindings),limits});
        state_=std::make_shared<const State>(State{std::move(fixed),declaration.revision,std::move(branches),{},{},{},nullptr,retained});
    }
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const {
        const auto pin=state_;const auto authority=context;
        authority.recheck(pin->fixed->origin);
        for(std::size_t i=0;i<pin->fixed->candidates.size();++i){if(check)check();
            pin->fixed->candidates[i].evidence.admit(authority,check);authority.recheck(pin->fixed->bindings[i]);}
        for(std::size_t i=0;i<pin->observations.size();++i){const auto& o=pin->observations[i];
            o.measured.admit(authority,check);for(const auto& input:o.inputs)input.input.admit(authority,check);
            authority.recheck(pin->origins[i]);authority.recheck(pin->bindings[i]);}
    }
    PreparedRelationalAssignmentEvidence append(std::vector<RelationalAssignmentObservation> suffix,
        const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const {
        using namespace assignment_evidence_detail;
        const auto pin=state_;const auto authority=context;Guard guard(pin->fixed->limits,check);
        const auto parent=*this;parent.admit(authority,check);require(!suffix.empty(),"nonempty measured solve suffix required");
        require(pin->revision!=std::numeric_limits<std::uint64_t>::max(),"assignment revision overflow");
        std::set<std::uint64_t> keys;for(const auto& o:pin->observations)keys.insert(o.key);
        auto origins=pin->origins,bindings=pin->bindings;
        // Admit every new original measurement and candidate input before any
        // new response, view or constraint solve is executed.
        for(const auto& o:suffix){
            guard.step();require(o.key&&keys.insert(o.key).second&&o.inputs.size()==pin->fixed->candidates.size(),"complete distinct measured assignment observation required");
            o.measured.admit(authority,check);const auto measured=o.measured.result().result();
            bindings.push_back(admit_binding(o.binding,measured.program(),authority));
            origins.push_back(authority.require(o.origin,Role::TargetValue,relational_assignment_measurement_payload(o)));
            for(std::size_t i=0;i<o.inputs.size();++i){
                require(o.inputs[i].candidate==pin->fixed->candidates[i].key,"candidate input order/identity differs");
                o.inputs[i].input.admit(authority,check);same_input(o.measured,o.inputs[i].input,guard);
            }
        }
        guard.entries(machine::checked_size_mul(pin->retained,2));
        auto branches=pin->branches;std::size_t retained=pin->retained;
        for(const auto& o:suffix){
            const auto target=response_reading(RelationalTaskResponse(o.measured),o.binding,guard,pin->fixed->limits,check);
            require(target.field.directions.cols()==0,"measurement must be an actual returned execution field");
            for(std::size_t i=0;i<branches.size();++i){
                guard.step();auto& branch=branches[i];if(branch.rejected)continue;
                const auto& c=pin->fixed->candidates[i];same_frame(c.binding,o.binding);
                auto response=c.evidence.execute(o.inputs[i].input,authority,pin->fixed->limits,check);
                const auto coordinates=compose({c.shift,c.injection},branch.coordinates,guard);
                const auto view=response_reading(RelationalTaskResponse(response),c.binding,guard,pin->fixed->limits,check);
                require(view.metric==target.metric,"measurement and candidate transported metrics differ");
                auto residual=restrict_field(view.field,coordinates,guard);
                auto r=residual.particular.values().dense();const auto y=target.field.particular.values().dense();
                require(r.size()==y.size(),"whole measured field carrier differs");
                for(std::size_t j=0;j<r.size();++j)r[j]=guard.sub(r[j],y[j]);
                residual.particular=tensor(r,guard);auto certificate=solve(std::move(residual),guard);
                if(certificate.status==RelationalTaskPostconditionStatus::Refuted)branch.rejected=true;
                else branch.coordinates=compose(branch.coordinates,{certificate.feasible_shift,certificate.feasible_kernel},guard);
                retained=add(retained,add(certificate.residual.particular.domain().logical_size(),certificate.residual.directions.nonzeros()));
                retained=add(retained,add(add(certificate.feasible_shift.size(),certificate.feasible_kernel.nonzeros()),certificate.contradiction.size()));
                const auto native=response.result();
                const auto program=native.evidence().program();
                for(const auto& node:program.declaration().nodes){const auto& f=native.field(node.key);
                    retained=add(retained,add(f.particular.domain().logical_size(),f.directions.nonzeros()));
                    if(node.kind==inference::FieldProgramNodeKind::Boundary){const auto& flags=native.occurrence_field(node.key);
                        retained=add(retained,add(flags.particular.domain().logical_size(),flags.directions.nonzeros()));}}
                retained=add(retained,add(branch.coordinates.shift.size(),branch.coordinates.map.nonzeros()));guard.entries(retained);
                branch.solves.push_back(std::make_shared<const RelationalAssignmentSolve>(RelationalAssignmentSolve{o.key,std::move(response),std::move(certificate),branch.coordinates}));
            }
        }
        auto observations=pin->observations;observations.insert(observations.end(),std::make_move_iterator(suffix.begin()),std::make_move_iterator(suffix.end()));
        guard.dimension(observations.size());
        // Retained ancestors share immutable solve owners. Charge this version's
        // coordinate/index copies and only the newly owned field certificates.
        for(const auto& branch:branches)retained=add(retained,add(branch.solves.size(),add(branch.coordinates.shift.size(),branch.coordinates.map.nonzeros())));
        retained=add(retained,machine::checked_size_mul(observations.size(),add(1,pin->fixed->candidates.size())));guard.entries(retained);
        return PreparedRelationalAssignmentEvidence(std::make_shared<const State>(State{pin->fixed,pin->revision+1,
            std::move(branches),std::move(observations),std::move(origins),std::move(bindings),pin,retained}));
    }
    std::optional<std::size_t> identified_candidate(const inference::RelationalAvailabilityContext& context) const {
        admit(context);if(!state_->fixed->remainder.empty())return {};
        std::optional<std::size_t> found;
        for(std::size_t i=0;i<state_->branches.size();++i)if(!state_->branches[i].rejected){if(found)return {};found=i;}
        return found;
    }
    RelationalAssignmentStatus status(const inference::RelationalAvailabilityContext& context) const {
        admit(context);std::size_t count{};for(const auto& b:state_->branches)if(!b.rejected)++count;
        if(!state_->fixed->remainder.empty()||count>1)return RelationalAssignmentStatus::Unresolved;
        return count?RelationalAssignmentStatus::Identified:RelationalAssignmentStatus::Inconsistent;
    }
    assignment_evidence_detail::Origin root_origin() const {return state_->fixed->origin;}
    std::vector<assignment_evidence_detail::Origin> dependency_origins() const {
        using namespace assignment_evidence_detail;const auto pin=state_;std::map<std::uint64_t,Origin> all;
        const auto add_origin=[&](const Origin& origin){const auto [it,inserted]=all.emplace(origin.key(),origin);
            require(inserted||it->second.same_owner(origin),"different actual origin owners reuse a dependency key");};
        add_origin(pin->fixed->origin);
        for(std::size_t i=0;i<pin->fixed->candidates.size();++i){add_origin(pin->fixed->bindings[i]);
            for(const auto& origin:pin->fixed->candidates[i].evidence.obligations())add_origin(origin);}
        for(std::size_t i=0;i<pin->observations.size();++i){const auto& o=pin->observations[i];add_origin(pin->origins[i]);add_origin(pin->bindings[i]);
            for(const auto& origin:o.measured.obligations())add_origin(origin);
            for(const auto& input:o.inputs)for(const auto& origin:input.input.obligations())add_origin(origin);}
        std::vector<Origin> out;for(const auto& [key,origin]:all){(void)key;out.push_back(origin);}return out;
    }
    const RelationalAssignmentBranch& branch(std::size_t index,const inference::RelationalAvailabilityContext& context) const & {
        admit(context);return state_->branches.at(index);
    }
    const RelationalAssignmentBranch& branch(std::size_t,const inference::RelationalAvailabilityContext&) const &&=delete;
    const std::vector<RelationalAssignmentCandidate>& candidates() const & {return state_->fixed->candidates;}
    const std::vector<RelationalAssignmentCandidate>& candidates() const &&=delete;
    const std::vector<RelationalAssignmentObservation>& observations() const & {return state_->observations;}
    const std::vector<RelationalAssignmentObservation>& observations() const &&=delete;
    const RelationalAssignmentDeclaration& declaration() const & {return state_->fixed->declaration;}
    const RelationalAssignmentDeclaration& declaration() const &&=delete;
    const std::vector<std::vector<std::uint64_t>>& remainder() const & {return state_->fixed->remainder;}
    const std::vector<std::vector<std::uint64_t>>& remainder() const &&=delete;
    inference::OwnedRelationalFiniteSource finite_source() const {return state_->fixed->finite;}
    numeric::ExactActionLimits limits() const {return state_->fixed->limits;}
    std::uint64_t revision() const noexcept {return state_->revision;}
    std::size_t retained_entries() const noexcept {return state_->retained;}
    bool same_owner(const PreparedRelationalAssignmentEvidence& other) const noexcept {return state_==other.state_;}
    std::optional<PreparedRelationalAssignmentEvidence> parent() const {if(state_->parent)return PreparedRelationalAssignmentEvidence(state_->parent);return {};}
    RelationalAssignmentResponse execute(std::size_t candidate,inference::AvailableRelationalFieldResult input,
        const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const;
private:
    explicit PreparedRelationalAssignmentEvidence(std::shared_ptr<const State> state):state_(std::move(state)){}
    std::shared_ptr<const State> state_;
};
class RelationalAssignmentResponse {
public:
    PreparedRelationalAssignmentEvidence model() const {return model_;}
    AvailableRelationalAffineResult original() const {return original_;}
    std::size_t candidate() const noexcept {return candidate_;}
    void admit(const inference::RelationalAvailabilityContext& context,std::function<void()> check={}) const {
        const auto pin=*this;const auto authority=context;pin.model_.admit(authority,check);pin.original_.admit(authority,check);
    }
    RelationalAssignmentCoordinates coordinates() const {return coordinates_;}
    RelationalAffineField field(std::uint64_t node,const inference::RelationalAvailabilityContext& context) const {
        admit(context);numeric::ExactActionGuard guard(model_.limits());const auto result=original_.result();
        return assignment_evidence_detail::restrict_field(result.field(node),coordinates_,guard);
    }
    RelationalAffineField occurrence_field(std::uint64_t node,const inference::RelationalAvailabilityContext& context) const {
        admit(context);numeric::ExactActionGuard guard(model_.limits());const auto result=original_.result();
        return assignment_evidence_detail::restrict_field(result.occurrence_field(node),coordinates_,guard);
    }
    RelationalTaskPostconditionCertificate postcondition(const OwnedRelationalTaskOutcome& goal,
        const inference::RelationalAvailabilityContext& context) const {
        using namespace assignment_evidence_detail;const auto pin=*this;const auto requested=goal;const auto authority=context;
        pin.admit(authority);requested.admit(authority);const auto response=requested.response();
        const auto* family=std::get_if<AvailableRelationalAffineResult>(&response);
        require(family&&family->evidence().evidence().same_owner(pin.original_.evidence().evidence())&&
            family->input().result().same_owner(pin.original_.input().result()),
            "task postcondition must retain this actual original response family");
        Guard guard(pin.model_.limits());const auto original=requested.certificate(authority);
        return solve(restrict_field(original.residual,pin.coordinates_,guard),guard);
    }
    RelationalAssignmentOccurrenceMap coefficient_independent_map(core::Grade grade,std::size_t flag,
        const inference::RelationalAvailabilityContext& context) const {
        using namespace assignment_evidence_detail;admit(context);Guard guard(model_.limits());
        const auto evidence=original_.evidence().evidence();const auto original=evidence.coefficient_independent_map(grade,flag,model_.limits());
        RelationalAssignmentOccurrenceMap out{original.particular(),std::vector<Matrix>(coordinates_.map.cols(),Matrix(original.particular().rows(),original.particular().cols()))};
        for(std::size_t i=0;i<original.directions().size();++i)for(const auto& [ij,q]:original.directions()[i].entries()){
            guard.step();out.particular.set(ij.first,ij.second,guard.add(out.particular.get(ij.first,ij.second),guard.mul(q,coordinates_.shift.at(i))));
            for(std::size_t j=0;j<out.directions.size();++j){guard.step();out.directions[j].set(ij.first,ij.second,
                guard.add(out.directions[j].get(ij.first,ij.second),guard.mul(q,coordinates_.map.get(i,j))));}
        }
        return out;
    }
private:
    friend class PreparedRelationalAssignmentEvidence;
    RelationalAssignmentResponse(PreparedRelationalAssignmentEvidence model,std::size_t candidate,
        AvailableRelationalAffineResult original,RelationalAssignmentCoordinates coordinates)
        :model_(std::move(model)),candidate_(candidate),original_(std::move(original)),coordinates_(std::move(coordinates)){}
    PreparedRelationalAssignmentEvidence model_;
    std::size_t candidate_;
    AvailableRelationalAffineResult original_;
    RelationalAssignmentCoordinates coordinates_;
};
inline RelationalAssignmentResponse PreparedRelationalAssignmentEvidence::execute(std::size_t candidate,
    inference::AvailableRelationalFieldResult input,const inference::RelationalAvailabilityContext& context,std::function<void()> check) const {
    using namespace assignment_evidence_detail;const auto pin=*this;const auto authority=context;Guard guard(pin.limits(),check);
    pin.admit(authority,check);const auto& b=pin.branch(candidate,authority);require(!b.rejected,"refuted worker assignment cannot execute as a compatible family");
    const auto& c=pin.candidates().at(candidate);auto original=c.evidence.execute(std::move(input),authority,pin.limits(),check);
    auto coordinates=compose({c.shift,c.injection},b.coordinates,guard);
    return RelationalAssignmentResponse(pin,candidate,std::move(original),std::move(coordinates));
}
} // namespace rfc::learning
