#pragma once
#include "rfc/core/relational_complex.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <array>
#include <map>
#include <optional>
#include <set>

namespace rfc::algebra {
// A type's three semantic roles are explicit signature-owned aspects. Neither
// boundary coefficients nor occurrence order are used to infer these roles.
struct FiniteBinaryOperation {
    std::uint64_t key{},revision{};
    core::RelationTypeId type{};
    composite::AspectId left{},right{},output{};
};
struct FiniteOperationWitness {
    core::RelationIdentity relation;
    std::array<std::size_t,3> slots;
    std::array<core::OccurrenceKey,3> occurrences;
    std::array<core::IdentityKey,3> values;
};
struct FiniteOperationProfile {
    bool functional{true},total{};
    std::size_t retained_relations{},input_pairs{};
    std::optional<std::pair<core::IdentityKey,core::IdentityKey>> missing;
    std::optional<std::array<core::RelationIdentity,2>> conflict;
};
struct FiniteOperationEvaluation {
    std::optional<core::IdentityKey> value;
    std::vector<FiniteOperationWitness> evidence;
};
enum class FiniteAlgebraLaw {
    Associative,Commutative,Idempotent,LeftIdentity,RightIdentity,
    LeftDistributive,RightDistributive
};
enum class FiniteLawStatus { Satisfied,Refuted,Undetermined };
struct FiniteLawRequest {
    FiniteAlgebraLaw law;
    std::size_t operation{},other{};
    // Required only for an identity law; it is an actual grade-zero identity.
    std::optional<core::IdentityKey> identity{};
};
struct FiniteLawInstance {
    std::vector<core::IdentityKey> assignment;
    FiniteOperationEvaluation left,right;
};
struct FiniteLawResult {
    FiniteLawStatus status{FiniteLawStatus::Satisfied};
    std::size_t assignments_checked{};
    std::optional<FiniteLawInstance> witness;
};
struct FiniteInverseWitness {
    core::IdentityKey element{},inverse{};
    FiniteOperationEvaluation left,right;
};

// Checks properties of the complete retained relation set on the declared
// grade-zero carrier. Duplicate assertions are retained as separate evidence;
// conflicting outputs remain a failed functionality condition. No table or
// operation is recovered from an aggregated rational boundary matrix.
class PreparedFiniteAlgebra {
    using Guard=numeric::ExactActionGuard;
    using Key=core::IdentityKey;
    using Pair=std::pair<Key,Key>;
    using Table=std::map<Pair,std::vector<FiniteOperationWitness>>;
    struct State {
        core::RelationalComplex source;
        std::vector<FiniteBinaryOperation> operations;
        std::vector<Table> tables;
        std::vector<FiniteOperationProfile> profiles;
        std::set<Key> carrier;
        numeric::ExactActionLimits limits;
        std::size_t retained{},steps{};
        State(core::RelationalComplex c,std::vector<FiniteBinaryOperation> o,numeric::ExactActionLimits l)
            :source(std::move(c)),operations(std::move(o)),tables(operations.size()),profiles(operations.size()),limits(l){}
    };
    std::shared_ptr<const State> state_;
    explicit PreparedFiniteAlgebra(std::shared_ptr<const State> state):state_(std::move(state)){}
    static std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
    static std::size_t mul(std::size_t a,std::size_t b){return machine::checked_size_mul(a,b);}
    void operation(Guard& guard,std::size_t scratch=0) const {
        const auto& a=guard.limits();const auto& b=state_->limits;
        if(a.dimension!=b.dimension || a.entries!=b.entries || a.steps!=b.steps || a.integer_bits!=b.integer_bits)
            throw std::invalid_argument("finite algebra action requires its original exact action limits");
        guard.step();guard.entries(add(state_->retained,scratch));
    }
    void require_element(Key key) const {
        if(!state_->carrier.contains(key))throw std::invalid_argument("finite algebra value is not a declared carrier identity");
    }
    FiniteOperationEvaluation evaluate(std::size_t op,Key left,Key right,Guard& guard) const {
        guard.step();const auto& table=state_->tables.at(op);const auto it=table.find({left,right});
        if(it==table.end())return {};
        FiniteOperationEvaluation out;
        guard.entries(add(state_->retained,mul(it->second.size(),12)));
        bool conflict=false;
        for(const auto& row:it->second) {
            guard.step();out.evidence.push_back(row);
            if(!out.value)out.value=row.values[2];else if(*out.value!=row.values[2])conflict=true;
        }
        if(conflict)out.value.reset();
        return out;
    }
    FiniteOperationEvaluation compose(std::size_t op,FiniteOperationEvaluation left,
            FiniteOperationEvaluation right,Guard& guard) const {
        guard.step();FiniteOperationEvaluation out;
        if(left.value && right.value)out=evaluate(op,*left.value,*right.value,guard);
        guard.entries(add(state_->retained,mul(add(out.evidence.size(),add(left.evidence.size(),right.evidence.size())),12)));
        out.evidence.insert(out.evidence.end(),left.evidence.begin(),left.evidence.end());
        out.evidence.insert(out.evidence.end(),right.evidence.begin(),right.evidence.end());return out;
    }
    static FiniteOperationEvaluation scalar(Key value){return {value,{}};}
    static std::size_t arity(FiniteAlgebraLaw law) {
        switch(law) {
            case FiniteAlgebraLaw::Associative:case FiniteAlgebraLaw::LeftDistributive:
            case FiniteAlgebraLaw::RightDistributive:return 3;
            case FiniteAlgebraLaw::Commutative:return 2;
            case FiniteAlgebraLaw::Idempotent:case FiniteAlgebraLaw::LeftIdentity:
            case FiniteAlgebraLaw::RightIdentity:return 1;
        }
        throw std::invalid_argument("unknown finite algebra law");
    }
    FiniteLawInstance instance(const FiniteLawRequest& request,std::vector<Key> a,Guard& guard) const {
        FiniteLawInstance out{std::move(a),{},{} };const auto& x=out.assignment;
        const auto f=[&](std::size_t op,Key l,Key r){return evaluate(op,l,r,guard);};
        const auto op=request.operation;
        switch(request.law) {
            case FiniteAlgebraLaw::Associative:
                out.left=compose(op,f(op,x[0],x[1]),scalar(x[2]),guard);
                out.right=compose(op,scalar(x[0]),f(op,x[1],x[2]),guard);break;
            case FiniteAlgebraLaw::Commutative:
                out.left=f(op,x[0],x[1]);out.right=f(op,x[1],x[0]);break;
            case FiniteAlgebraLaw::Idempotent:
                out.left=f(op,x[0],x[0]);out.right=scalar(x[0]);break;
            case FiniteAlgebraLaw::LeftIdentity:
                out.left=f(op,*request.identity,x[0]);out.right=scalar(x[0]);break;
            case FiniteAlgebraLaw::RightIdentity:
                out.left=f(op,x[0],*request.identity);out.right=scalar(x[0]);break;
            case FiniteAlgebraLaw::LeftDistributive:
                out.left=compose(op,scalar(x[0]),f(request.other,x[1],x[2]),guard);
                out.right=compose(request.other,f(op,x[0],x[1]),f(op,x[0],x[2]),guard);break;
            case FiniteAlgebraLaw::RightDistributive:
                out.left=compose(op,f(request.other,x[0],x[1]),scalar(x[2]),guard);
                out.right=compose(request.other,f(op,x[0],x[2]),f(op,x[1],x[2]),guard);break;
        }
        return out;
    }
public:
    class LawCheck {
        friend class PreparedFiniteAlgebra;
        std::shared_ptr<const State> owner_;
        FiniteLawRequest request_;
        FiniteLawResult result_;
        LawCheck(std::shared_ptr<const State> owner,FiniteLawRequest request,FiniteLawResult result)
            :owner_(std::move(owner)),request_(std::move(request)),result_(std::move(result)){}
    public:
        const FiniteLawRequest& request() const & {return request_;}
        const FiniteLawResult& result() const & {return result_;}
        const core::RelationalComplex& source() const & {return owner_->source;}
        const FiniteLawRequest& request() const &&=delete;
        const FiniteLawResult& result() const &&=delete;
        const core::RelationalComplex& source() const &&=delete;
    };
    class FieldCheck {
        friend class PreparedFiniteAlgebra;
        std::shared_ptr<const State> owner_;
        std::size_t addition_,multiplication_;
        Key zero_,one_;
        std::vector<LawCheck> laws_;
        std::vector<FiniteInverseWitness> additive_,multiplicative_;
        std::vector<Key> missing_additive_,missing_multiplicative_;
        bool valid_{};
        FieldCheck(std::shared_ptr<const State> owner,std::size_t addition,std::size_t multiplication,Key zero,Key one)
            :owner_(std::move(owner)),addition_(addition),multiplication_(multiplication),zero_(zero),one_(one){}
    public:
        bool valid() const noexcept {return valid_;}
        Key zero() const noexcept {return zero_;}
        Key one() const noexcept {return one_;}
        std::size_t addition() const noexcept {return addition_;}
        std::size_t multiplication() const noexcept {return multiplication_;}
        const std::vector<LawCheck>& laws() const & {return laws_;}
        const std::vector<FiniteInverseWitness>& additive_inverses() const & {return additive_;}
        const std::vector<FiniteInverseWitness>& multiplicative_inverses() const & {return multiplicative_;}
        const std::vector<Key>& missing_additive_inverses() const & {return missing_additive_;}
        const std::vector<Key>& missing_multiplicative_inverses() const & {return missing_multiplicative_;}
        const core::RelationalComplex& source() const & {return owner_->source;}
        const std::vector<LawCheck>& laws() const &&=delete;
        const std::vector<FiniteInverseWitness>& additive_inverses() const &&=delete;
        const std::vector<FiniteInverseWitness>& multiplicative_inverses() const &&=delete;
        const std::vector<Key>& missing_additive_inverses() const &&=delete;
        const std::vector<Key>& missing_multiplicative_inverses() const &&=delete;
        const core::RelationalComplex& source() const &&=delete;
    };

    static PreparedFiniteAlgebra prepare(const core::RelationalComplex& source,
            std::vector<FiniteBinaryOperation> operations,Guard& guard) {
        const auto start=guard.steps();guard.step();
        if(source.grade_count()<2 || !source.grade(core::Grade{0}).cell_count() || operations.empty())
            throw std::invalid_argument("finite algebra needs a nonempty retained carrier and declared operations");
        guard.dimension(source.grade(core::Grade{0}).cell_count());guard.dimension(operations.size());
        std::size_t words=mul(operations.size(),16);
        for(std::size_t k=0;k<source.grade_count();++k) {
            guard.step();const auto& arena=source.grade(core::Grade{static_cast<std::uint16_t>(k)});
            words=add(words,add(mul(arena.cell_count(),24),mul(arena.incidence_count(),add(16,source.signature().aspects().size()))));
            guard.entries(mul(words,2));for(const auto& q:arena.cell_scales){guard.step();guard.value(q);}
            for(const auto& z:arena.lattice_coefficient){guard.step();guard.value(numeric::Rational{z});}
        }
        auto state=std::make_shared<State>(source,std::move(operations),guard.limits());state->retained=words;
        std::map<core::RelationTypeId,std::size_t> selected;std::set<std::uint64_t> keys;
        for(std::size_t i=0;i<state->operations.size();++i) {
            guard.step();const auto& op=state->operations[i];(void)source.signature().type(op.type);
            if(!op.key || !op.revision || !keys.insert(op.key).second || !selected.emplace(op.type,i).second)
                throw std::invalid_argument("finite operation identities, revisions and type meanings must be distinct");
            const std::set<composite::AspectId> roles{op.left,op.right,op.output};
            if(roles.size()!=3)throw std::invalid_argument("finite operation needs three distinct role aspects");
            for(const auto role:roles)if(source.signature().aspects().at(role).role!=composite::AspectRole::RelationRole)
                throw std::invalid_argument("finite operation roles must be signature-owned relation-role aspects");
        }
        for(const auto key:source.grade(core::Grade{0}).identities){guard.step();state->carrier.insert(key);}
        const auto& arena=source.grade(core::Grade{1});const auto exists=source.signature().existence_aspect();
        for(std::size_t cell=0;cell<arena.cell_count();++cell) {
            guard.step();const auto found=selected.find(arena.relation_types[cell]);if(found==selected.end())continue;
            const auto index=found->second;const auto& op=state->operations[index];const auto [begin,end]=arena.incidence_range(cell);
            if(end-begin!=3)throw std::invalid_argument("finite binary operation assertion must retain exactly three role occurrences");
            FiniteOperationWitness row{source.relation_identity({core::Grade{1},static_cast<std::uint32_t>(cell)}),{},{},{}};
            const std::array<composite::AspectId,3> roles{op.left,op.right,op.output};std::array<bool,3> seen{};
            for(std::size_t p=begin;p<end;++p) {
                guard.step();if(!arena.aspects.get(exists,p))throw std::invalid_argument("finite operation assertion contains an inactive role occurrence");
                std::optional<std::size_t> selected_role;
                for(std::size_t r=0;r<3;++r)if(arena.aspects.get(roles[r],p)) {
                    if(selected_role)throw std::invalid_argument("finite operation occurrence has multiple semantic roles");
                    selected_role=r;
                }
                if(!selected_role || seen[*selected_role])throw std::invalid_argument("finite operation role is missing or repeated");
                const auto r=*selected_role;seen[r]=true;row.slots[r]=p;row.occurrences[r]=source.occurrence_identity(core::Grade{1},p);
                row.values[r]=source.grade(core::Grade{0}).identities.at(arena.lower_index[p]);
            }
            auto& bucket=state->tables[index][{row.values[0],row.values[1]}];auto& profile=state->profiles[index];
            if(!bucket.empty() && bucket.front().values[2]!=row.values[2]) {
                profile.functional=false;if(!profile.conflict)profile.conflict=std::array{bucket.front().relation,row.relation};
            }
            state->retained=add(state->retained,16);guard.entries(mul(state->retained,2));bucket.push_back(std::move(row));++profile.retained_relations;
        }
        const auto total_pairs=mul(state->carrier.size(),state->carrier.size());
        for(std::size_t i=0;i<state->operations.size();++i) {
            guard.step();auto& profile=state->profiles[i];profile.input_pairs=state->tables[i].size();profile.total=profile.input_pairs==total_pairs;
            if(!profile.total) {
                for(const auto left:state->carrier) {
                    for(const auto right:state->carrier){guard.step();if(!state->tables[i].contains({left,right})){profile.missing=Pair{left,right};break;}}
                    if(profile.missing)break;
                }
            }
        }
        state->steps=guard.steps()-start;return PreparedFiniteAlgebra(std::move(state));
    }
    const core::RelationalComplex& source() const & {return state_->source;}
    const std::vector<FiniteBinaryOperation>& operations() const & {return state_->operations;}
    const std::vector<FiniteOperationProfile>& profiles() const & {return state_->profiles;}
    const std::set<Key>& carrier() const & {return state_->carrier;}
    const core::RelationalComplex& source() const &&=delete;
    const std::vector<FiniteBinaryOperation>& operations() const &&=delete;
    const std::vector<FiniteOperationProfile>& profiles() const &&=delete;
    const std::set<Key>& carrier() const &&=delete;
    bool same_owner(const PreparedFiniteAlgebra& other) const noexcept {return state_==other.state_;}
    std::size_t retained_entries() const noexcept {return state_->retained;}
    std::size_t preparation_steps() const noexcept {return state_->steps;}

    FiniteOperationEvaluation apply(std::size_t operation_index,Key left,Key right,Guard& guard) const {
        operation(guard);require_element(left);require_element(right);return evaluate(operation_index,left,right,guard);
    }
    LawCheck check_law(FiniteLawRequest request,Guard& guard) const {
        // Two finite expression traces are live together. Even when repeated
        // assertions share a value, each expression retains its own evidence.
        operation(guard,mul(state_->retained,8));(void)state_->operations.at(request.operation);const auto n=arity(request.law);
        if(request.law==FiniteAlgebraLaw::LeftDistributive || request.law==FiniteAlgebraLaw::RightDistributive)
            (void)state_->operations.at(request.other);
        if(request.law==FiniteAlgebraLaw::LeftIdentity || request.law==FiniteAlgebraLaw::RightIdentity) {
            if(!request.identity)throw std::invalid_argument("identity law requires a declared carrier identity");
            require_element(*request.identity);
        }
        std::size_t count=1;for(std::size_t i=0;i<n;++i)count=mul(count,state_->carrier.size());
        if(count>guard.limits().steps-guard.steps())throw std::length_error("finite law assignment work admission");
        const std::vector<Key> values(state_->carrier.begin(),state_->carrier.end());FiniteLawResult result;
        for(std::size_t index=0;index<count;++index) {
            guard.step();std::size_t rest=index;std::vector<Key> assignment(n);
            for(std::size_t j=0;j<n;++j){assignment[j]=values[rest%values.size()];rest/=values.size();}
            auto current=instance(request,std::move(assignment),guard);++result.assignments_checked;
            if(!current.left.value || !current.right.value) {
                if(result.status==FiniteLawStatus::Satisfied){result.status=FiniteLawStatus::Undetermined;result.witness=std::move(current);}
            } else if(*current.left.value!=*current.right.value) {
                result.status=FiniteLawStatus::Refuted;result.witness=std::move(current);break;
            }
        }
        return LawCheck(state_,std::move(request),std::move(result));
    }
    bool owns(const LawCheck& receipt) const noexcept {return state_==receipt.owner_;}
    bool owns(const FieldCheck& receipt) const noexcept {return state_==receipt.owner_;}
    FieldCheck check_field(std::size_t addition,std::size_t multiplication,Key zero,Key one,Guard& guard) const {
        operation(guard);require_element(zero);require_element(one);
        if(addition==multiplication)throw std::invalid_argument("field validation requires separately declared addition and multiplication");
        const auto& add_profile=state_->profiles.at(addition);const auto& mul_profile=state_->profiles.at(multiplication);
        FieldCheck out(state_,addition,multiplication,zero,one);
        std::size_t receipt_entries=16;
        const auto retain=[&](std::size_t count) {
            receipt_entries=add(receipt_entries,count);
            guard.entries(add(mul(state_->retained,9),receipt_entries));
        };
        const auto law=[&](FiniteAlgebraLaw kind,std::size_t op,std::size_t other=0,std::optional<Key> identity={}) {
            auto result=check_law({kind,op,other,identity},guard);
            std::size_t count=12;
            if(const auto& witness=result.result().witness)count=add(count,add(witness->assignment.size(),
                mul(add(witness->left.evidence.size(),witness->right.evidence.size()),12)));
            retain(count);out.laws_.push_back(std::move(result));
        };
        for(const auto op:{addition,multiplication}) {
            const auto identity=op==addition?zero:one;
            law(FiniteAlgebraLaw::Associative,op);law(FiniteAlgebraLaw::Commutative,op);
            law(FiniteAlgebraLaw::LeftIdentity,op,0,identity);law(FiniteAlgebraLaw::RightIdentity,op,0,identity);
        }
        law(FiniteAlgebraLaw::LeftDistributive,multiplication,addition);
        law(FiniteAlgebraLaw::RightDistributive,multiplication,addition);
        const auto inverses=[&](std::size_t op,Key identity,bool skip_zero,std::vector<FiniteInverseWitness>& found,std::vector<Key>& missing) {
            for(const auto x:state_->carrier) {
                guard.step();if(skip_zero && x==zero)continue;bool exists=false;
                for(const auto y:state_->carrier) {
                    auto left=evaluate(op,x,y,guard),right=evaluate(op,y,x,guard);
                    if(left.value==identity && right.value==identity) {
                        retain(add(4,mul(add(left.evidence.size(),right.evidence.size()),12)));
                        found.push_back({x,y,std::move(left),std::move(right)});exists=true;break;
                    }
                }
                if(!exists){retain(1);missing.push_back(x);}
            }
        };
        inverses(addition,zero,false,out.additive_,out.missing_additive_);
        inverses(multiplication,one,true,out.multiplicative_,out.missing_multiplicative_);
        out.valid_=zero!=one && add_profile.functional && add_profile.total && mul_profile.functional && mul_profile.total &&
            out.missing_additive_.empty() && out.missing_multiplicative_.empty() &&
            std::all_of(out.laws_.begin(),out.laws_.end(),[](const auto& check){return check.result().status==FiniteLawStatus::Satisfied;});
        return out;
    }
};
} // namespace rfc::algebra
