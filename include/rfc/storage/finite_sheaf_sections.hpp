#pragma once
#include "rfc/storage/sheaf_cover_gluing.hpp"
#include "rfc/local/constraint_system.hpp"
#include <numeric>

namespace rfc::storage {
// A catalogue is an explicitly FINITE family of TOTAL local degree-zero cochains.
// Its keys are alternative identities, not numerical weights. Equal fields can
// have different keys. Missing observations constrain nothing; they are not zero.
struct FiniteSheafCandidate { std::uint64_t key{}; local::SparseSheafCochain field; };
struct FiniteSheafCatalogue { std::string label; std::vector<FiniteSheafCandidate> alternatives; };
struct FiniteSheafObservation {
    std::string label;
    std::size_t patch{};
    DeclaredStalkBinding stalk;
    numeric::SparseRationalTensor reading; // support is exactly observed coverage
};
struct FiniteSheafCondition {
    std::string label;
    std::vector<std::size_t> patches;
    std::vector<std::vector<std::uint64_t>> allowed;
};
enum class FiniteSheafConditionKind { Interface, Section, Observation, Supplied };
struct FiniteSheafClause {
    FiniteSheafCondition condition;
    FiniteSheafConditionKind kind{FiniteSheafConditionKind::Supplied};
};
struct FiniteSheafCoverage {
    std::string label;
    std::size_t patch{}, dimension{}, observed{}, numerical_nonzeros{};
    StableCellKey cell;
};
struct FiniteSheafFamilyLimits {
    std::size_t patches{4096}, alternatives{65536}, clauses{65536};
    std::size_t tuple_entries{1u<<22}, label_bytes{1u<<20};
};
struct FiniteSheafQuery {
    local::FiniteSearchStatus status{local::FiniteSearchStatus::BudgetExceeded};
    std::optional<std::vector<std::uint64_t>> witness;
    std::size_t steps{}, solver_steps{}, value_trials{}, tuple_tests{};
    std::shared_ptr<const void> family_owner;
    std::map<std::size_t,std::uint64_t> fixed;
};
struct FiniteSheafConflict {
    local::FiniteSearchStatus status{local::FiniteSearchStatus::BudgetExceeded};
    std::vector<std::size_t> clauses;
    // Witness i satisfies all retained core clauses EXCEPT clauses[i].
    std::vector<std::vector<std::uint64_t>> deletion_witnesses;
    bool subset_minimal{false};
    std::size_t steps{};
    std::shared_ptr<const void> family_owner;
};

// Exact finite feasibility over the declared maps of an observation-gluing view.
// Retains the complete catalogue/condition family, not independent marginals.
// No whole product of candidate assignments is constructed. Local pair tables
// ARE explicit, are charged before allocation and can exhaust the budget.
class FiniteSheafSectionFamily {
    using Q=numeric::Rational; using S=numeric::SparseRationalVector;
    using G=numeric::ExactActionGuard; using System=local::FiniteConstraintSystem<std::uint64_t>;
    static constexpr core::Grade zero{0};
    struct State {
        SheafCoverLocalFieldView view;
        core::OwnedRelationalSource choices;
        std::vector<core::CellId> selectors;
        std::vector<FiniteSheafCatalogue> catalogues;
        std::vector<FiniteSheafClause> clauses;
        std::vector<FiniteSheafCoverage> coverage;
        std::vector<FiniteSheafObservation> observations;
        std::vector<std::vector<std::optional<local::SparseSheafCochain>>> residuals;
        bool sections{};
        std::size_t records{}, preparation_steps{};
    };
    std::shared_ptr<const State> state_;
    static void require(bool b,const char* m){if(!b)throw std::invalid_argument(m);}
    static const FiniteSheafCandidate& candidate(const State& s,std::size_t p,std::uint64_t key){
        for(const auto& c:s.catalogues.at(p).alternatives)if(c.key==key)return c;
        throw std::invalid_argument("finite sheaf candidate key not declared");
    }
    static S value(const local::SparseSheafCochain& f,core::CellId c,std::size_t n){
        const auto it=f.values().find(c.index);return it==f.values().end()?S(n):it->second;
    }
    static Q coefficient(const S& s,std::size_t i){
        const auto at=std::lower_bound(s.entries().begin(),s.entries().end(),i,[](const auto& e,auto index){return e.index<index;});
        return at!=s.entries().end() && at->index==i?at->value:Q{0};
    }
    struct Interrupted {local::FiniteSearchStatus status;};
    struct Budget {
        local::FiniteSearchLimits limits; std::stop_token stop;
        std::size_t used{},solver_steps{},trials{},tuples{};
        void charge(std::size_t n=1){
            if(stop.stop_requested())throw Interrupted{local::FiniteSearchStatus::Cancelled};
            if(n>limits.max_steps-used)throw Interrupted{local::FiniteSearchStatus::BudgetExceeded};
            used+=n;
        }
    };
    static FiniteSheafQuery search(const std::shared_ptr<const State>& s,
        const std::vector<std::size_t>& active,const std::map<std::size_t,std::uint64_t>& fixed,Budget& b){
        b.charge();
        if(s->selectors.size()>b.limits.max_variables)throw Interrupted{local::FiniteSearchStatus::BudgetExceeded};
        System system(s->choices.source()); typename System::Assignment assignment;
        for(std::size_t p=0;p<s->catalogues.size();++p){
            std::vector<std::uint64_t> keys;
            const auto size=s->catalogues[p].alternatives.size();
            b.charge(machine::checked_size_add(machine::checked_size_mul(size,size),size+1));
            for(const auto& c:s->catalogues[p].alternatives)keys.push_back(c.key);
            system.set_domain(s->selectors[p],std::move(keys));
        }
        for(const auto& [p,key]:fixed){b.charge();require(p<s->selectors.size(),"fixed patch index out of range");
            b.charge(s->catalogues[p].alternatives.size());(void)candidate(*s,p,key);assignment.emplace(s->selectors[p],key);}
        for(const auto i:active){
            b.charge();const auto& c=s->clauses.at(i).condition;
            if(c.allowed.empty())return {local::FiniteSearchStatus::Unsatisfiable,{},b.used,b.solver_steps,b.trials,b.tuples,s,fixed};
            std::vector<core::CellId> support;algebra::SubcomplexMask patch(s->choices.source());
            b.charge(c.patches.size());
            for(const auto p:c.patches){support.push_back(s->selectors[p]);patch.select(s->selectors[p]);}
            std::size_t per_tuple=c.patches.size();
            for(const auto p:c.patches)per_tuple=machine::checked_size_add(per_tuple,s->catalogues[p].alternatives.size());
            b.charge(machine::checked_size_mul(c.allowed.size(),per_tuple));
            system.add_chart(c.label,std::move(patch),std::move(support),c.allowed);
        }
        auto limit=b.limits;limit.max_steps-=b.used;
        const auto result=system.bounded_global_section(assignment,limit,b.stop);
        b.used+=result.work.steps;b.solver_steps+=result.work.steps;b.trials+=result.work.value_trials;b.tuples+=result.work.tuple_tests;
        if(result.status==local::FiniteSearchStatus::Cancelled || result.status==local::FiniteSearchStatus::BudgetExceeded)
            throw Interrupted{result.status};
        std::optional<std::vector<std::uint64_t>> witness;
        if(result.witness){witness.emplace();b.charge(s->selectors.size());for(const auto c:s->selectors)witness->push_back(result.witness->at(c));}
        return {result.status,std::move(witness),b.used,b.solver_steps,b.trials,b.tuples,s,fixed};
    }
    static std::vector<std::size_t> all(const State& s){std::vector<std::size_t> x(s.clauses.size());std::iota(x.begin(),x.end(),0);return x;}
public:
    FiniteSheafSectionFamily(SheafCoverLocalFieldView view,core::OwnedRelationalSource choices,
        std::vector<core::CellId> selectors,std::vector<FiniteSheafCatalogue> catalogues,
        std::vector<FiniteSheafObservation> observations={},std::vector<FiniteSheafCondition> conditions={},
        bool require_sections=true,FiniteSheafFamilyLimits bounds={},numeric::ExactActionLimits limits={},
        std::function<void()> checkpoint={}) {
        G g(limits,std::move(checkpoint));g.entries(view.retained_records());
        require(!catalogues.empty() && catalogues.size()==view.patch_count() && selectors.size()==catalogues.size(),"finite sheaf needs one catalogue and selector per patch");
        if(catalogues.size()>bounds.patches)throw std::length_error("finite sheaf patch budget");
        require(choices.source().grade_count()==1 && choices.source().grade(zero).cell_count()==selectors.size(),"choice source must contain exactly the declared degree-zero selector cells");
        auto s=std::make_shared<State>(State{std::move(view),std::move(choices),std::move(selectors),std::move(catalogues),{},{},{},{},require_sections,0,0});
        s->records=machine::checked_size_add(s->view.retained_records(),s->selectors.size());g.entries(s->records);
        std::set<core::CellId> unique;std::set<std::string> labels;std::size_t count=0,bytes=0,tuples=0;
        const auto label=[&](const std::string& str){bytes=machine::checked_size_add(bytes,str.size());if(bytes>bounds.label_bytes)throw std::length_error("finite sheaf label budget");require(!str.empty() && labels.insert(str).second,"finite sheaf labels must be unique");};
        const auto clause=[&](FiniteSheafCondition c,FiniteSheafConditionKind kind){
            if(s->clauses.size()>=bounds.clauses)throw std::length_error("finite sheaf clause budget");
            label(c.label);
            const auto n=machine::checked_size_mul(c.allowed.size(),c.patches.size());tuples=machine::checked_size_add(tuples,n);
            if(tuples>bounds.tuple_entries)throw std::length_error("finite sheaf tuple budget");
            s->records=machine::checked_size_add(s->records,machine::checked_size_add(c.patches.size()+1,n));g.entries(s->records);s->clauses.push_back({std::move(c),kind});
        };
        for(std::size_t p=0;p<s->catalogues.size();++p){
            require(s->selectors[p].grade==zero && s->selectors[p].index<s->selectors.size() && unique.insert(s->selectors[p]).second,"choice selector identity/repetition");
            const auto& cat=s->catalogues[p];label(cat.label);require(!cat.alternatives.empty(),"empty catalogue is not a finite declared domain");
            count=machine::checked_size_add(count,cat.alternatives.size());if(count>bounds.alternatives)throw std::length_error("finite sheaf alternative budget");
            std::set<std::uint64_t> keys;std::vector<std::optional<local::SparseSheafCochain>> residuals;
            FiniteSheafCondition valid{"section/"+cat.label,{p},{}};
            const auto& patch=s->view.patches()[p];std::optional<local::SparseSheafActions> action;
            if(require_sections && s->view.grade_count()>1)action.emplace(patch.sheaf(),limits);
            for(const auto& c:cat.alternatives){
                g.step();require(keys.insert(c.key).second,"duplicate candidate identity");
                require(c.field.grade()==zero && c.field.owner().same_owner(patch.sheaf()),"candidate has foreign sheaf owner or grade");
                for(const auto& [cell,values]:c.field.values()){(void)cell;g.sparse_vector(values);}
                s->records=machine::checked_size_add(s->records,1+c.field.nonzeros());g.entries(s->records);
                std::optional<local::SparseSheafCochain> residual;
                if(action){auto r=action->coboundary(c.field);for(std::size_t t=0;t<r.work.checked_steps;++t)g.step();residual=std::move(r.field);
                    s->records=machine::checked_size_add(s->records,residual->nonzeros());g.entries(s->records);}
                if(!residual || !residual->nonzeros())valid.allowed.push_back({c.key});
                residuals.push_back(std::move(residual));
            }
            s->residuals.push_back(std::move(residuals));if(require_sections)clause(std::move(valid),FiniteSheafConditionKind::Section);
        }
        // Retain one binary constraint per actual interface cell. Tables are
        // built from the declared local-to-common maps, never raw coordinate equality.
        std::size_t cell_index=0;
        for(const auto& cell:s->view.layout(zero)){
            std::size_t li=0;
            for(const auto& link:cell.interfaces){
                const auto& lo=cell.certificate.origins.at(link.left);const auto& ro=cell.certificate.origins.at(link.right);
                const auto lp=lo.patch,rp=ro.patch;
                const auto n=machine::checked_size_mul(s->catalogues[lp].alternatives.size(),s->catalogues[rp].alternatives.size());
                if(n> (bounds.tuple_entries-tuples)/2)throw std::length_error("finite sheaf interface pair budget");
                std::array<std::vector<S>,2> images;std::size_t temporary=s->records;
                for(std::size_t side=0;side<2;++side){const auto& o=side?ro:lo;const auto& patch=s->view.patches()[o.patch];const auto c=sheaf_cover_detail::matched(patch,o.local);
                    for(const auto& option:s->catalogues[o.patch].alternatives){g.step();auto v=value(option.field,c,o.local.carrier.domain.logical_size());auto image=numeric::checked_apply(link.restrictions[side],v,g);temporary=machine::checked_size_add(temporary,1+image.nonzeros());g.entries(temporary);images[side].push_back(std::move(image));}
                }
                FiniteSheafCondition c{"interface/"+std::to_string(cell_index)+"/"+std::to_string(li++),{lp,rp},{}};
                for(std::size_t i=0;i<images[0].size();++i)for(std::size_t j=0;j<images[1].size();++j){g.step();
                    for(std::size_t t=0;t<images[0][i].nonzeros()+images[1][j].nonzeros();++t)g.step();
                    if(images[0][i].entries()==images[1][j].entries())c.allowed.push_back({s->catalogues[lp].alternatives[i].key,s->catalogues[rp].alternatives[j].key});}
                clause(std::move(c),FiniteSheafConditionKind::Interface);
            }++cell_index;
        }
        for(const auto& obs:observations){
            require(obs.patch<s->catalogues.size(),"observation patch out of range");const auto& patch=s->view.patches()[obs.patch];const auto cell=sheaf_cover_detail::matched(patch,obs.stalk);
            require(cell.grade==zero && obs.reading.domain()==obs.stalk.carrier.domain,"observation grade/domain differs");g.sparse_vector(obs.reading.values());
            const auto n=obs.reading.domain().logical_size();
            const auto& support=obs.reading.support();const auto support_records=support.sparse_indices()?support.sparse_indices()->size():support.packed_bits()?n/64+std::size_t(n%64!=0):1;
            s->records=machine::checked_size_add(s->records,machine::checked_size_add(support_records,obs.reading.values().nonzeros()+1));g.entries(s->records);
            for(std::size_t i=0;i<support_records;++i)g.step();
            s->coverage.push_back({obs.label,obs.patch,n,obs.reading.support().count(),obs.reading.values().nonzeros(),*obs.stalk.carrier.cell});
            FiniteSheafCondition c{obs.label,{obs.patch},{}};
            for(const auto& a:s->catalogues[obs.patch].alternatives){g.step();const auto v=value(a.field,cell,n);bool equal=true;
                for(const auto& e:v.entries()){g.step();if(obs.reading.support().contains(e.index) && coefficient(obs.reading.values(),e.index)!=e.value)equal=false;}
                for(const auto& e:obs.reading.values().entries()){g.step();if(coefficient(v,e.index)!=e.value)equal=false;}
                if(equal)c.allowed.push_back({a.key});}
            clause(std::move(c),FiniteSheafConditionKind::Observation);
        }
        for(auto& c:conditions){
            require(!c.patches.empty(),"finite condition has empty support");std::set<std::size_t> ps;
            for(const auto p:c.patches)require(p<s->catalogues.size() && ps.insert(p).second,"finite condition patch identity/repetition");
            for(const auto& tuple:c.allowed){g.step();require(tuple.size()==c.patches.size(),"finite condition tuple arity");for(std::size_t i=0;i<tuple.size();++i){for(std::size_t j=0;j<s->catalogues[c.patches[i]].alternatives.size();++j)g.step();(void)candidate(*s,c.patches[i],tuple[i]);}}
            clause(std::move(c),FiniteSheafConditionKind::Supplied);
        }
        s->observations=std::move(observations);
        s->preparation_steps=g.steps();state_=std::move(s);
    }
    const auto& catalogues()const &{return state_->catalogues;}const auto& catalogues()const &&=delete;
    const auto& clauses()const &{return state_->clauses;}const auto& clauses()const &&=delete;
    const auto& coverage()const &{return state_->coverage;}const auto& coverage()const &&=delete;
    const auto& candidate_residuals()const &{return state_->residuals;}const auto& candidate_residuals()const &&=delete;
    const auto& observations()const &{return state_->observations;}const auto& observations()const &&=delete;
    bool requires_sections()const{return state_->sections;}
    std::size_t preparation_steps()const{return state_->preparation_steps;}
    FiniteSheafQuery query(const std::map<std::size_t,std::uint64_t>& fixed={},local::FiniteSearchLimits limits={},std::stop_token stop={})const{
        const auto s=state_;Budget b{limits,stop};try{b.charge(s->clauses.size());return search(s,all(*s),fixed,b);}catch(const Interrupted& e){return {e.status,{},b.used,b.solver_steps,b.trials,b.tuples,s,fixed};}
    }
    // Bounded deletion-based subset-minimal conflict, relative to the declared
    // finite catalogues. No minimum-cardinality or physical-cause claim. Every
    // retained clause has a checked necessity witness; interruptions return no core.
    FiniteSheafConflict conflict(local::FiniteSearchLimits limits={},std::stop_token stop={})const{
        const auto s=state_;Budget b{limits,stop};try{
            b.charge(s->clauses.size());auto active=all(*s);const auto initial=search(s,active,{},b);
            if(initial.status!=local::FiniteSearchStatus::Unsatisfiable)return {initial.status,{},{},false,b.used,s};
            std::size_t i=0;while(i<active.size()){b.charge(active.size());auto trial=active;trial.erase(trial.begin()+static_cast<std::ptrdiff_t>(i));
                if(search(s,trial,{},b).status==local::FiniteSearchStatus::Unsatisfiable)active=std::move(trial);else ++i;}
            std::vector<std::vector<std::uint64_t>> witnesses;
            for(i=0;i<active.size();++i){b.charge(active.size());auto trial=active;trial.erase(trial.begin()+static_cast<std::ptrdiff_t>(i));const auto r=search(s,trial,{},b);
                if(r.status!=local::FiniteSearchStatus::Satisfiable || !r.witness)throw std::logic_error("finite sheaf conflict witness check failed");
                witnesses.push_back(*r.witness);}
            return {local::FiniteSearchStatus::Unsatisfiable,std::move(active),std::move(witnesses),true,b.used,s};
        }catch(const Interrupted& e){return {e.status,{},{},false,b.used,s};}
    }
    S glue_assignment(const std::vector<std::uint64_t>& keys,G& g)const{
        const auto s=state_;require(keys.size()==s->catalogues.size(),"one key per local catalogue required");std::vector<local::SparseSheafCochain> fields;
        for(std::size_t p=0;p<keys.size();++p){g.step();fields.push_back(candidate(*s,p,keys[p]).field);}
        for(const auto& clause:s->clauses){const auto& c=clause.condition;bool found=false;
            for(const auto& t:c.allowed){g.step();bool match=true;for(std::size_t i=0;i<t.size();++i){g.step();if(t[i]!=keys[c.patches[i]])match=false;}if(match){found=true;break;}}
            require(found,"assignment violates retained finite condition");}
        auto out=s->view.glue_fields(zero,fields,g);
        if(s->sections && s->view.grade_count()>1 && !s->view.coboundary(zero).apply(out,g).empty())throw std::logic_error("finite section does not have zero global coboundary");
        return out;
    }
};
} // namespace rfc::storage
