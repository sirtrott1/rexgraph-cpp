#pragma once
#include "rfc/storage/demand_cover.hpp"
#include "rfc/database/native_payload_cache.hpp"
#include "rfc/numeric/prepared_exact_frames.hpp"
#include <numeric>

namespace rfc::inference {
struct DemandExecutionLimits {
    std::size_t max_work_bytes{512u<<20},max_output_bytes{64u<<20},max_patch_visits{100000};
    std::size_t max_coordinates{65536},max_rows{4096},max_boolean_candidates{65536};
    numeric::ExactActionLimits exact{65536,4000000,16000000,4096};
};
struct DemandIndexQualification {
    bool verified{};
    std::size_t patches{},payload_io_bytes{},work_envelope_bytes{},scalar_steps{};
};
struct DemandExecutionTrace {
    std::vector<storage::PayloadKey> loaded;
    std::vector<std::uint64_t> generations;
    std::size_t patch_visits{},occurrence_visits{},scalar_steps{},work_envelope_bytes{},output_bytes{},fill_entries{},output_coefficient_bits{};
    bool global{};
    DemandIndexQualification index_preparation;
};
struct DemandExecutionResult {
    std::uint16_t grade{};std::size_t width{1};
    std::map<std::uint64_t,numeric::RationalVector> field;
    numeric::Rational moment{};
    std::map<std::uint64_t,numeric::Rational> channels;
    std::vector<std::uint64_t> variables,edges;
    numeric::RationalVector particular,contradiction;
    numeric::ExactMatrix equations{0,0},kernel{0,0};
    std::vector<numeric::RationalVector> boolean_members;
    bool consistent{},boolean_complete{};
    DemandExecutionTrace trace;
};
struct DemandField {
    storage::DemandCover owner;
    std::uint64_t identity{};std::uint16_t grade{};std::size_t width{1};
    // This is a declared TOTAL field. Omitted numerical coordinates are zero;
    // missing observations must not be passed as this declaration.
    std::map<std::uint64_t,numeric::RationalVector> values;
};

class DemandGluing {
    using Q=numeric::Rational;using V=numeric::RationalVector;using M=numeric::ExactMatrix;
    using Key=storage::PayloadKey;using Cell=storage::PayloadCell;using Field=std::map<std::uint64_t,V>;
    using Guard=numeric::ExactActionGuard;
    mutable database::NativePayloadCache cache_;storage::DemandCover cover_;DemandExecutionLimits limits_;
    struct IndexState {std::mutex mutex;bool preparing{};DemandIndexQualification receipt;};
    std::shared_ptr<IndexState> index_{std::make_shared<IndexState>()};
    static std::size_t add(std::size_t a,std::size_t b){return machine::checked_size_add(a,b);}
    static std::size_t mul(std::size_t a,std::size_t b){return machine::checked_size_mul(a,b);}
    static void require(bool x,const char* text){storage::payload_manifest_detail::require(x,text);}
    struct Run {
        const DemandGluing& owner;DemandExecutionResult result;Guard guard;std::function<void()> check;
        std::optional<std::vector<std::byte>> signature;std::optional<core::SemanticScale> scale;
        std::size_t peak_named_slots{},signature_bytes{};
        Run(const DemandGluing& o,std::function<void()> c):owner(o),guard(o.limits_.exact,c),check(std::move(c)){}
        void workspace(std::size_t slots) {
            peak_named_slots=std::max(peak_named_slots,slots);
            const auto bytes=add(signature_bytes,mul(peak_named_slots,add(256,numeric::rational_limb_envelope(owner.limits_.exact.integer_bits))));
            storage::payload_manifest_detail::bound(bytes,owner.limits_.max_work_bytes,"demand exact workspace admission");
            result.trace.work_envelope_bytes=std::max(result.trace.work_envelope_bytes,bytes);guard.entries(slots);
        }
        void record(const database::NativePayloadCache::Selection& selection) {
            for(const auto& lease:selection.members()) {
                const auto at=std::find(result.trace.loaded.begin(),result.trace.loaded.end(),lease.key());
                if(at==result.trace.loaded.end()){workspace(add(256,mul(add(result.trace.loaded.size(),1),32)));result.trace.loaded.push_back(lease.key());result.trace.generations.push_back(lease.generation());}
            }
        }
        database::NativePayloadCache::Selection load(std::size_t pi) {
            guard.step();if(result.trace.patch_visits==owner.limits_.max_patch_visits)throw std::length_error("demand patch visit admission");
            ++result.trace.patch_visits;const auto& patch=owner.cover_.declaration().patches.at(pi);
            require(patch.available_at<=owner.cover_.declaration().cutoff,"demand payload is unavailable at selected evidence cutoff");
            std::vector<Key> pending{patch.payload};std::set<Key> seen;
            for(std::size_t j=0;j<pending.size();++j){guard.step();const auto key=pending[j];if(!seen.insert(key).second)continue;
                const auto at=std::find_if(owner.cover_.declaration().patches.begin(),owner.cover_.declaration().patches.end(),[&](const auto& p){return p.payload==key;});
                require(at!=owner.cover_.declaration().patches.end() && at->available_at<=owner.cover_.declaration().cutoff,"demand dependency is outside its selected availability scope");
                const auto& dependencies=owner.cover_.manifest().payloads()[*owner.cover_.manifest().find(key)].dependencies;
                for(const auto& dependency:dependencies)if(!seen.contains(dependency))pending.push_back(dependency);
                workspace(add(256,mul(pending.size(),16)));}
            auto selected=owner.cache_.acquire(patch.payload,patch.payload.source,check);record(selected);
            const auto& source=selected.root().source();require(source.grade_count()<=2,"demand graph source profile");
            // The canonical raw signature is already retained/validated by the
            // native lease. Borrow it rather than allocating a second encoder.
            storage::rfcx_detail::Reader header(selected.root().bytes(),40);
            const auto offset=header.size_value(),size=header.size_value();
            const auto bytes=selected.root().bytes().subspan(offset,size);
            if(signature)require(std::ranges::equal(*signature,bytes) && *scale==source.semantic_scale(),"demand participants change declaration environment/semantic scale");
            else {signature_bytes=add(sizeof(std::vector<std::byte>),mul(size,3));workspace(0);
                signature.emplace(bytes.begin(),bytes.end());require(signature->capacity()<=signature_bytes,"demand signature capacity envelope");scale=source.semantic_scale();}
            return selected;
        }
        // Read and compare the FULL original fiber for every declared copy of
        // this shared upper cell. Duplicate copies are sections, not extra flux.
        std::map<std::uint64_t,Q> row(std::uint64_t edge) {
            const auto found=owner.cover_.cells().find(Cell{{1},edge});require(found!=owner.cover_.cells().end(),"unknown demand edge");
            using Occ=std::tuple<std::uint64_t,std::uint64_t,Q,std::vector<bool>>;
            std::optional<std::vector<Occ>> baseline;std::optional<core::RelationTypeId> type;std::map<std::uint64_t,Q> output;
            for(const auto& [pi,ci]:found->second) {
                auto selected=load(pi);const auto& c=selected.root().source();const auto& patch=owner.cover_.declaration().patches[pi];
                const auto& declaration=owner.cover_.manifest().payloads()[*owner.cover_.manifest().find(patch.payload)];
                const auto local=declaration.cells[ci];const auto& arena=c.grade({1});std::size_t index{};
                for(;index<arena.cell_count();++index){guard.step();if(arena.relation_identities[index].value==local.key)break;}
                require(index<arena.cell_count(),"demand original edge absent after native reconstruction");
                if(type)require(*type==arena.relation_types[index],"shared cell type changes");else type=arena.relation_types[index];
                workspace(add(128,mul(add(patch.cells.size(),patch.occurrences.size()),32)));
                std::map<Cell,Cell> mapped;for(std::size_t j=0;j<patch.cells.size();++j)mapped.emplace(declaration.cells[j],patch.cells[j]);
                std::map<std::uint64_t,std::size_t> positions;for(std::size_t j=0;j<declaration.occurrences.size();++j)positions.emplace(declaration.occurrences[j].key.value,j);
                std::vector<Occ> occurrences;const auto [begin,end]=arena.incidence_range(index);workspace(add(64,mul(end-begin,16)));
                for(auto p=begin;p<end;++p) {
                    guard.step();++result.trace.occurrence_visits;
                    const auto lower=mapped.at(Cell{{0},c.grade({0}).identities[arena.lower_index[p]]}).key;
                    const auto target=patch.occurrences[positions.at(arena.occurrence_identities[p].value)];
                    std::vector<bool> aspects;for(std::size_t a=0;a<c.signature().aspects().size();++a)aspects.push_back(arena.aspects.get(static_cast<composite::AspectId>(a),p));
                    const auto q=numeric::canonical_rational(c.incidence_coefficient({1},p));guard.value(q);
                    occurrences.emplace_back(target,lower,q,std::move(aspects));
                    if(!baseline && arena.aspects.get(c.signature().existence_aspect(),p))output[lower]=guard.add(output[lower],q);
                }
                if(baseline)require(*baseline==occurrences,"shared occurrence coefficient/aspect data disagree");else baseline=std::move(occurrences);
            }
            std::erase_if(output,[](const auto& x){return x.second==0;});return output;
        }
        void put(Field& f,std::uint64_t key,std::size_t coordinate,const Q& value,std::size_t width) {
            auto it=f.find(key);if(it==f.end()){workspace(add(128,mul(add(f.size(),1),mul(width,8))));it=f.emplace(key,V(width)).first;}
            it->second[coordinate]=guard.add(it->second[coordinate],value);
        }
        Field action(const Field& input,bool transpose,std::size_t width) {
            Field out;std::set<std::uint64_t> edges;
            if(transpose)for(const auto& [v,values]:input){(void)values;const auto it=owner.cover_.adjacency().find(v);if(it!=owner.cover_.adjacency().end())edges.insert(it->second.begin(),it->second.end());}
            else for(const auto& [e,values]:input){(void)values;edges.insert(e);}
            workspace(add(256,mul(add(edges.size(),input.size()),mul(width,16))));
            for(const auto edge:edges) {
                const auto coefficients=row(edge);
                for(const auto& [v,q]:coefficients){const auto at=input.find(transpose?v:edge);if(at==input.end())continue;
                    for(std::size_t j=0;j<width;++j)put(out,transpose?edge:v,j,guard.mul(q,at->second[j]),width);}
            }
            std::erase_if(out,[](const auto& item){return numeric::is_zero(item.second);});return out;
        }
        void finish() {
            result.trace.index_preparation=owner.index_qualification();
            require(result.trace.index_preparation.verified,"unqualified demand topology cannot issue a query result");
            const auto count=[&](const Q& q){const auto bits=[](mpz_srcptr z){return mpz_sgn(z)?mpz_sizeinbase(z,2):std::size_t{0};};
                result.trace.output_coefficient_bits=add(result.trace.output_coefficient_bits,add(bits(mpq_numref(q.get_mpq_t())),bits(mpq_denref(q.get_mpq_t()))));};
            count(result.moment);
            for(const auto& [key,v]:result.field){(void)key;for(const auto& q:v)count(q);}
            for(const auto& [key,q]:result.channels){(void)key;count(q);}
            for(const auto* v:{&result.particular,&result.contradiction})for(const auto& q:*v)count(q);
            for(const auto* m:{&result.kernel,&result.equations})for(const auto& [ij,q]:m->entries()){(void)ij;count(q);}
            for(const auto& v:result.boolean_members)for(const auto& q:v)count(q);
            auto slots=add(result.field.size(),add(result.particular.size(),result.contradiction.size()));
            slots=add(slots,add(result.kernel.nonzeros(),result.equations.nonzeros()));
            for(const auto& [k,v]:result.field){(void)k;slots=add(slots,v.size());}
            for(const auto& v:result.boolean_members)slots=add(slots,v.size());
            slots=add(slots,add(result.variables.size(),add(result.edges.size(),mul(result.trace.loaded.size(),8))));
            slots=add(slots,mul(result.channels.size(),2));
            result.trace.output_bytes=mul(add(slots,64),add(256,numeric::rational_limb_envelope(owner.limits_.exact.integer_bits)));
            storage::payload_manifest_detail::bound(result.trace.output_bytes,owner.limits_.max_output_bytes,"demand output admission");
            result.trace.scalar_steps=guard.steps();if(check){check();}
        }
    };
public:
    DemandGluing(database::NativePayloadCache cache,storage::DemandCover cover,DemandExecutionLimits limits={})
        :cache_(std::move(cache)),cover_(std::move(cover)),limits_(limits) {
        require(std::ranges::equal(cache_.manifest().bytes(),cover_.manifest().bytes()),"demand cover uses a different original payload manifest");
    }
    const storage::DemandCover& cover() const & {return cover_;}
    const storage::DemandCover& cover() const &&=delete;
    DemandIndexQualification index_qualification() const {std::lock_guard lock(index_->mutex);return index_->receipt;}
    // A passive manifest cannot prove that an UNLOADED payload has no hidden
    // incidence. Stream every selected original source once, validate its FULL
    // native metadata and shared fibers, then evict payloads. Only this actual
    // owner-bound qualification authorizes the metadata frontier. Reopen repeats
    // the pass; a checksum alone cannot restore its authority. Preparation is
    // global in the selected cover and charged separately from each local query.
    void qualify_index(std::function<void()> check={}) const {
        {
            std::lock_guard lock(index_->mutex);
            if(index_->receipt.verified)return;
            require(!index_->preparing,"demand index qualification already in progress");index_->preparing=true;
        }
        try {
            Run verification(*this,check);verification.workspace(256);
            const auto before=cache_.archive_usage().payload_io_bytes;
            for(const auto& p:cover_.declaration().patches){verification.guard.step();require(p.available_at<=cover_.declaration().cutoff,"demand index includes an unavailable source");}
            for(std::size_t p=0;p<cover_.declaration().patches.size();++p){auto selected=verification.load(p);(void)selected;}
            for(const auto& [cell,owners]:cover_.cells())if(cell.grade.value && owners.size()>1)(void)verification.row(cell.key);
            for(const auto& p:cover_.declaration().patches)(void)cache_.evict(p.payload);
            if(check){check();}
            DemandIndexQualification receipt{true,cover_.declaration().patches.size(),cache_.archive_usage().payload_io_bytes-before,
                verification.result.trace.work_envelope_bytes,verification.guard.steps()};
            std::lock_guard lock(index_->mutex);index_->receipt=receipt;index_->preparing=false;
        } catch(...) {std::lock_guard lock(index_->mutex);index_->preparing=false;throw;}
    }
    DemandField field(std::uint64_t identity,std::uint16_t grade,std::size_t width,Field values) const {
        require(identity && grade<=1 && width && width<=16,"demand field needs a total declared graph carrier/identity/width");
        storage::payload_manifest_detail::bound(mul(values.size(),width),limits_.max_coordinates,"demand field coordinate admission");
        for(auto& [key,v]:values){require(cover_.cells().contains(Cell{{grade},key}) && v.size()==width,"demand field original carrier/coordinate shape");for(auto& q:v)q=numeric::canonical_rational(q);}
        std::erase_if(values,[](const auto& x){return numeric::is_zero(x.second);});return {cover_,identity,grade,width,std::move(values)};
    }
    // Words execute from left to right: D boundary, U coboundary, L named-unit
    // Hodge at the current grade. No assembled power or all-pairs Gram is built.
    DemandExecutionResult word(const DemandField& input,std::string_view operations,bool channels=false,std::function<void()> check={}) const {
        require(input.owner.same_owner(cover_),"demand field has a foreign actual cover owner");
        storage::payload_manifest_detail::bound(operations.size(),4096,"demand operation word admission");
        storage::payload_manifest_detail::bound(mul(input.values.size(),input.width),limits_.max_coordinates,"demand input coordinate admission");
        Guard receiving(limits_.exact);for(const auto& [key,values]:input.values){(void)key;for(const auto& q:values)receiving.value(q);}
        // Freeze caller data and word before invoking any cancellation callback.
        const auto frozen=field(input.identity,input.grade,input.width,input.values);const std::string word(operations);
        qualify_index(check);
        Run run(*this,std::move(check));run.result.grade=frozen.grade;run.result.width=frozen.width;auto current=frozen.values;
        run.workspace(add(256,mul(current.size(),mul(frozen.width,16))));
        for(const auto& [key,v]:current){(void)v;(void)run.load(cover_.cells().at(Cell{{frozen.grade},key}).front().first);}
        for(const auto& [key,v]:current){(void)key;run.guard.vector(v);}
        for(const auto op:word) {
            run.guard.step();if(op=='L')current=run.action(run.action(current,run.result.grade==0,frozen.width),run.result.grade!=0,frozen.width);
            else if(op=='D'){require(run.result.grade==1,"boundary word grade");current=run.action(current,false,frozen.width);run.result.grade=0;}
            else if(op=='U'){require(run.result.grade==0,"coboundary word grade");current=run.action(current,true,frozen.width);run.result.grade=1;}
            else throw std::invalid_argument("unknown demand operation word");
        }
        if(run.result.grade==frozen.grade)for(const auto& [key,v]:frozen.values){auto at=current.find(key);if(at!=current.end())for(std::size_t j=0;j<frozen.width;++j)run.result.moment=run.guard.add(run.result.moment,run.guard.mul(v[j],at->second[j]));}
        if(channels) {
            require(run.result.grade==0,"edge energy channels need a vertex field");
            const auto edge_values=run.action(current,true,frozen.width);
            for(const auto& [e,v]:edge_values){Q q;for(const auto& x:v)q=run.guard.add(q,run.guard.mul(x,x));run.result.channels[e]=q;}
        }
        run.result.field=std::move(current);run.finish();return std::move(run.result);
    }
    // Complete joint affine constraints B^T x = rhs. Connected scope expands to
    // a fixed point; global scope includes disconnected rows and isolated cells.
    // No arbitrary local fit or product of marginal solution sets is returned.
    DemandExecutionResult family(std::vector<std::uint64_t> seeds,const std::map<std::uint64_t,Q>& rhs,bool global,
        bool boolean=false,std::function<void()> check={}) const {
        storage::payload_manifest_detail::bound(seeds.size(),limits_.max_coordinates,"demand seed admission");
        storage::payload_manifest_detail::bound(rhs.size(),limits_.max_rows,"demand rhs admission");
        Guard receiving(limits_.exact);for(const auto& [key,q]:rhs){(void)key;receiving.value(q);}
        const auto original_rhs=rhs;
        const auto original_seeds=seeds;
        qualify_index(check);
        Run run(*this,std::move(check));run.result.trace.global=global;
        std::set<std::uint64_t> vertices(original_seeds.begin(),original_seeds.end()),edges;
        for(const auto v:vertices)require(cover_.cells().contains(Cell{{0},v}),"family seed is outside its actual declared cover");
        if(global){for(std::size_t p=0;p<cover_.declaration().patches.size();++p)(void)run.load(p);
            for(const auto& [c,owners]:cover_.cells()){(void)owners;(c.grade.value?edges:vertices).insert(c.key);}}
        bool changed=true;
        while(changed) {
            run.guard.step();changed=false;
            for(const auto v:std::vector<std::uint64_t>(vertices.begin(),vertices.end())) {
                const auto at=cover_.adjacency().find(v);if(at!=cover_.adjacency().end())for(auto e:at->second)if(edges.insert(e).second)changed=true;
            }
            for(const auto e:edges) {
                const auto& owner=cover_.cells().at(Cell{{1},e}).front();const auto& p=cover_.declaration().patches[owner.first];
                const auto& d=cover_.manifest().payloads()[*cover_.manifest().find(p.payload)];
                for(const auto& o:d.occurrences)if(o.upper==d.cells[owner.second] && o.exists) {
                    const auto at=std::find(d.cells.begin(),d.cells.end(),o.lower);const auto v=p.cells[static_cast<std::size_t>(at-d.cells.begin())].key;
                    if(vertices.insert(v).second)changed=true;
                }
            }
            storage::payload_manifest_detail::bound(vertices.size(),limits_.max_coordinates,"joint family variable admission");
            storage::payload_manifest_detail::bound(edges.size(),limits_.max_rows,"joint family row admission");
        }
        const auto n=vertices.size(),m=edges.size();run.workspace(mul(add(add(n,m),16),mul(add(add(n,m),16),16)));
        run.result.variables.assign(vertices.begin(),vertices.end());run.result.edges.assign(edges.begin(),edges.end());M a(m,n),augmented(m,add(n,1));V b(m);
        std::size_t i{};for(const auto edge:edges){const auto coefficients=run.row(edge);for(const auto& [v,q]:coefficients){const auto j=static_cast<std::size_t>(std::lower_bound(run.result.variables.begin(),run.result.variables.end(),v)-run.result.variables.begin());require(j<n && run.result.variables[j]==v,"joint dependency closure omitted a variable");a.set(i,j,q);augmented.set(i,j,q);}
            const auto value=original_rhs.find(edge);b[i]=value==original_rhs.end()?Q{0}:numeric::canonical_rational(value->second);run.guard.value(b[i]);augmented.set(i,n,b[i]);++i;}
        for(const auto& [edge,value]:original_rhs){(void)value;require(cover_.cells().contains(Cell{{1},edge}),"unknown original constraint rhs");}
        const numeric::PreparedExactFrames frame(a,run.guard),joint(augmented,run.guard);
        run.result.consistent=frame.rank()==joint.rank();run.result.equations=a;run.result.kernel=M(n,0);
        run.result.trace.fill_entries=add(frame.stored_entries(),joint.stored_entries());
        if(run.result.consistent) {
            run.result.particular=V(n);run.result.kernel=frame.kernel();
            for(std::size_t j=0;j<joint.rank();++j)run.result.particular[joint.pivots()[j]]=joint.reduced().get(j,n);
            require(numeric::checked_apply(a,run.result.particular,run.guard)==b && !numeric::checked_multiply(a,run.result.kernel,run.guard).nonzeros(),"joint affine family certificate failed");
        } else {
            const numeric::PreparedExactFrames dual(a.transpose(),run.guard);
            for(std::size_t j=0;j<dual.kernel().cols() && run.result.contradiction.empty();++j){V witness(m);Q q;for(const auto& [ij,x]:dual.kernel().entries())if(ij.second==j){witness[ij.first]=x;q=run.guard.add(q,run.guard.mul(x,b[ij.first]));}if(q!=0)run.result.contradiction=std::move(witness);}
            require(!run.result.contradiction.empty() && numeric::is_zero(numeric::checked_transpose_apply(a,run.result.contradiction,run.guard)),"joint contradiction certificate failed");
        }
        if(boolean) {
            require(n<std::numeric_limits<std::size_t>::digits,"Boolean joint carrier exceeds finite profile");
            const auto candidates=std::size_t{1}<<n;storage::payload_manifest_detail::bound(candidates,limits_.max_boolean_candidates,"complete Boolean family candidate admission");
            // Exhaust the WHOLE explicitly finite cube under the same exact
            // equation evaluator. Refusal/cancellation returns no partial family.
            run.workspace(add(mul(add(n,1),candidates),mul(add(m,n),16)));
            for(std::size_t bits=0;bits<candidates;++bits){V value(n);for(std::size_t j=0;j<n;++j)value[j]=(bits>>j)&1u;
                if(numeric::checked_apply(a,value,run.guard)==b)run.result.boolean_members.push_back(std::move(value));}
            run.result.boolean_complete=true;
        }
        run.finish();return std::move(run.result);
    }
};
} // namespace rfc::inference
