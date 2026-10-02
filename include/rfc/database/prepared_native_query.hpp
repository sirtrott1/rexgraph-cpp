#pragma once

#include "rfc/database/native_query.hpp"
#include "rfc/runtime/cpu_tiles.hpp"
#include "rfc/runtime/work_graph.hpp"
#include <span>
#include <bit>
#include <mutex>

namespace rfc::database {
enum class NativeQueryOutput { Keys, Aggregate };
enum class NativeQueryIndex {None,NumericRanges};
struct NativeQueryRequest {
    std::vector<ExactScalarPredicate> all_of;
    std::optional<std::vector<std::uint64_t>> sum;
    NativeQueryOutput output{NativeQueryOutput::Keys};
};
namespace prepared_query_detail {
inline std::size_t bits(const composite::DeclaredValue& value) {
    const auto integer=[](const numeric::Integer& n){return composite::declaration_detail::integer_bits(n);};
    if (const auto* n=std::get_if<numeric::Integer>(&value.data)) return integer(*n);
    if (const auto* q=std::get_if<numeric::Rational>(&value.data)) return std::max(integer(q->get_num()),integer(q->get_den()));
    if (const auto* o=std::get_if<composite::DeclaredValue::Optional>(&value.data)) {
        std::size_t maximum=0;for (const auto& item:o->element) maximum=std::max(maximum,bits(item));return maximum;
    }
    const auto sequence=[](const auto& values) {
        std::size_t maximum=0;for (const auto& item:values) maximum=std::max(maximum,bits(item));return maximum;
    };
    if (const auto* p=std::get_if<composite::DeclaredValue::Product>(&value.data)) return sequence(p->elements);
    if (const auto* a=std::get_if<composite::DeclaredValue::Alternative>(&value.data)) return sequence(a->element);
    if (const auto* r=std::get_if<composite::DeclaredValue::Repeated>(&value.data)) return sequence(r->elements);
    if (const auto* t=std::get_if<numeric::SparseRationalTensor>(&value.data)) {
        std::size_t maximum=0;
        for (const auto& entry:t->values().entries()) maximum=std::max(maximum,std::max(integer(entry.value.get_num()),integer(entry.value.get_den())));
        return maximum;
    }
    return 0;
}
inline std::size_t bits(const numeric::Rational& q) {
    return std::max(composite::declaration_detail::integer_bits(q.get_num()),composite::declaration_detail::integer_bits(q.get_den()));
}
inline void check_bits(const numeric::Rational& q,const runtime::ExecutionLease& lease) {
    const auto n=bits(q);
    if (n>lease.policy().limits.integer_bits) throw runtime::ExecutionLimitExceeded("query integer bits",n,lease.policy().limits.integer_bits);
}
struct Projection {
    std::vector<std::uint64_t> path;
    native_query_detail::Scalar scalar;
    std::vector<std::shared_ptr<const composite::DeclaredValue>> values;
    std::vector<std::size_t> ordered, null_rows;
};
inline const composite::DeclaredValue* element(const Projection& c,std::size_t row) {
    const auto* value=c.values[row].get();
    if(c.scalar.nullable) {
        const auto& option=std::get<composite::DeclaredValue::Optional>(value->data);
        return option.element.empty()?nullptr:&option.element.front();
    }
    return value;
}
inline int compare(const Projection& c,const composite::DeclaredValue& a,const composite::DeclaredValue& b) {
    if(c.scalar.kind==composite::DeclarationKind::Integer)
        return mpz_cmp(std::get<numeric::Integer>(a.data).get_mpz_t(),std::get<numeric::Integer>(b.data).get_mpz_t());
    return mpq_cmp(std::get<numeric::Rational>(a.data).get_mpq_t(),std::get<numeric::Rational>(b.data).get_mpq_t());
}
struct Plan {
    InterpretedSnapshot source;
    std::uint64_t kind;
    NativeQueryRequest request;
    std::vector<Projection> projections;
    std::vector<std::size_t> clauses;
    std::optional<std::size_t> sum_column;
    runtime::ExecutionRequirements requirements;
    std::size_t rows_per_tile,tiles{},payload_resolutions{};
    std::optional<std::vector<std::size_t>> candidates;
    std::size_t indexed_projections{};
    Plan(InterpretedSnapshot s,std::uint64_t k,NativeQueryRequest r,std::size_t rows)
        :source(std::move(s)),kind(k),request(std::move(r)),rows_per_tile(rows){}
};
struct Data {
    std::vector<std::uint64_t> keys;
    std::size_t selected{},unknown{},numeric_contributions{},predicate_evaluations{};
    std::optional<numeric::Rational> sum;
    runtime::CpuTileRunReport cpu;
};
} // namespace prepared_query_detail

class PreparedNativeQuery;
class NativeQueryCache;
struct QueryWorkOwnership;
class DerivedNativeQuery {
public:
    const std::vector<std::uint64_t>& keys() const & noexcept {return data_->keys;}
    const std::vector<std::uint64_t>& keys() const &&=delete;
    std::shared_ptr<const std::vector<std::uint64_t>> keys_pin() const {return {data_,&data_->keys};}
    std::size_t selected_count() const {return data_->selected;}
    std::size_t unknown_count() const {return data_->unknown;}
    std::size_t numeric_contributions() const {return data_->numeric_contributions;}
    std::size_t predicate_evaluations() const {return cache_hit_?0:data_->predicate_evaluations;}
    bool cache_hit() const {return cache_hit_;}
    const std::optional<numeric::Rational>& sum() const & {return data_->sum;}
    const std::optional<numeric::Rational>& sum() const &&=delete;
    const NativeQueryRequest& request() const & {return plan_->request;}
    const NativeQueryRequest& request() const &&=delete;
    runtime::CpuTileRunReport cpu_report() const {return cache_hit_?runtime::CpuTileRunReport{}:data_->cpu;}
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const {return plan_->source.endpoint();}
    runtime::WorkAvailability availability() const {return availability_;}
    std::uint64_t assertion_kind() const {return plan_->kind;}
    const InterpretationSchema& schema() const & {return plan_->source.schema();}
    const InterpretationSchema& schema() const &&=delete;
    const runtime::ExecutionHostPolicy& execution_policy() const & {return policy_;}
    const runtime::ExecutionHostPolicy& execution_policy() const &&=delete;
    bool cpu_fallback() const {return policy_.backend!=runtime::ExecutionBackend::ExactCpu;}
    std::size_t retained_object_bytes() const {
        return machine::checked_size_add(sizeof(DerivedNativeQuery)+sizeof(prepared_query_detail::Data),
            machine::checked_size_add(machine::checked_size_mul(data_->keys.capacity(),sizeof(std::uint64_t)),
                data_->sum?numeric::rational_limb_bytes(*data_->sum):0));
    }
private:
    DerivedNativeQuery(std::shared_ptr<const prepared_query_detail::Plan> plan,
        prepared_query_detail::Data data,runtime::WorkAvailability availability,runtime::ExecutionHostPolicy policy)
        :plan_(std::move(plan)),data_(std::make_shared<const prepared_query_detail::Data>(std::move(data))),availability_(availability),policy_(std::move(policy)){}
    std::shared_ptr<const prepared_query_detail::Plan> plan_;
    std::shared_ptr<const prepared_query_detail::Data> data_;
    runtime::WorkAvailability availability_;
    runtime::ExecutionHostPolicy policy_;
    bool cache_hit_{};
    friend class PreparedNativeQuery;
    friend struct QueryWorkOwnership;
    friend class NativeQueryCache;
};

// Pure predicates only. The sealed publication owns every resolved alias; the
// original ordered operations and retained assertion keys remain inspectable.
class PreparedNativeQuery {
public:
    static PreparedNativeQuery prepare(const InterpretedSnapshot& source,std::uint64_t kind,
        const NativeQueryRequest& request,runtime::WorkAvailability availability,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{},std::size_t rows_per_tile=256,
        NativeQueryIndex index=NativeQueryIndex::None) {
        lease.check_cancelled();
        runtime::admit_work_availability(source.schema().source_namespace,source.sequence(),availability);
        if (!rows_per_tile) throw std::invalid_argument("query tile size must be positive");
        if(index!=NativeQueryIndex::None && index!=NativeQueryIndex::NumericRanges)throw std::invalid_argument("unknown native query index");
        if (request.output!=NativeQueryOutput::Keys && request.output!=NativeQueryOutput::Aggregate)
            throw std::invalid_argument("unknown native query output contract");
        const auto rows=source.assertions(kind).size();
        const auto count=machine::checked_size_add(request.all_of.size(),request.sum?1:0);
        runtime::ExecutionRequirements required;
        required.records=machine::checked_size_add(rows,machine::checked_size_mul(rows,count));
        required.scalar_work=machine::checked_size_mul(rows,machine::checked_size_add(count,1));
        interpretation_detail::bound(required.scalar_work,source.limits().max_work,"interpretation query work budget");
        runtime::admit_execution(required,lease.policy());
        std::size_t literal_nodes=0,literal_bytes=0;
        for (const auto& p:request.all_of) {
            lease.check_cancelled();
            interpretation_detail::bound(p.factors.size(),source.limits().declarations.max_depth,"query factor depth");
            interpretation_detail::account_value_copy(p.literal,source.limits(),literal_nodes,literal_bytes);
            required.integer_bits=std::max(required.integer_bits,prepared_query_detail::bits(p.literal));
            required.records=machine::checked_size_add(required.records,machine::checked_size_add(p.factors.size(),1));
            required.scalar_work=machine::checked_size_add(required.scalar_work,machine::checked_size_mul(rows,p.factors.size()));
            const auto scalar=native_query_detail::scalar(source.schema(),kind,p.factors);
            native_query_detail::validate(source.schema(),scalar,p);
            if (p.operation!=ExactPredicateKind::IsNull && p.operation!=ExactPredicateKind::IsNotNull)
                required.integer_bits=std::max(required.integer_bits,prepared_query_detail::bits(p.literal));
        }
        if (request.sum) {
            interpretation_detail::bound(request.sum->size(),source.limits().declarations.max_depth,"query sum factor depth");
            required.records=machine::checked_size_add(required.records,machine::checked_size_add(request.sum->size(),1));
            required.scalar_work=machine::checked_size_add(required.scalar_work,machine::checked_size_mul(rows,request.sum->size()));
            const auto scalar=native_query_detail::scalar(source.schema(),kind,*request.sum);
            native_query_detail::require(scalar.kind==composite::DeclarationKind::Integer || scalar.kind==composite::DeclarationKind::Rational,
                "query sum requires a declared numeric scalar");
        }
        const auto tiles=machine::ceil_div(rows,rows_per_tile);
        required.records=machine::checked_size_add(required.records,literal_nodes);
        interpretation_detail::bound(required.scalar_work,source.limits().max_work,"interpretation query binding work budget");
        required.rational_slots=request.sum?machine::checked_size_add(tiles,2):0;
        required.records=machine::checked_size_add(required.records,machine::checked_size_add(tiles,request.output==NativeQueryOutput::Keys?rows:0));
        runtime::admit_execution(required,lease.policy());
        auto plan=std::make_shared<prepared_query_detail::Plan>(source,kind,request,rows_per_tile);
        plan->tiles=tiles;
        const auto column=[&](const std::vector<std::uint64_t>& path) {
            for (std::size_t i=0;i<plan->projections.size();++i) if (plan->projections[i].path==path) return i;
            prepared_query_detail::Projection projection{path,native_query_detail::scalar(source.schema(),kind,path),{},{},{}};
            projection.values.reserve(rows);
            for (const auto key:source.assertions(kind)) {
                lease.check_cancelled();
                auto value=source.payload(key,path).declared_value();
                required.integer_bits=std::max(required.integer_bits,prepared_query_detail::bits(*value));
                projection.values.push_back(std::move(value));++plan->payload_resolutions;
            }
            plan->projections.push_back(std::move(projection));return plan->projections.size()-1;
        };
        for (const auto& p:request.all_of) plan->clauses.push_back(column(p.factors));
        if (request.sum) plan->sum_column=column(*request.sum);
        if(index==NativeQueryIndex::NumericRanges) {
            for(auto& c:plan->projections) {
                if(c.scalar.kind!=composite::DeclarationKind::Integer && c.scalar.kind!=composite::DeclarationKind::Rational)continue;
                required.records=machine::checked_size_add(required.records,rows);
                required.scalar_work=machine::checked_size_add(required.scalar_work,machine::checked_size_mul(rows,static_cast<std::size_t>(std::bit_width(rows))+1));
                interpretation_detail::bound(required.scalar_work,source.limits().max_work,"query index work budget");
                runtime::admit_execution(required,lease.policy());
                for(std::size_t row=0;row<rows;++row) {
                    lease.check_cancelled();
                    (prepared_query_detail::element(c,row)?c.ordered:c.null_rows).push_back(row);
                }
                std::stable_sort(c.ordered.begin(),c.ordered.end(),[&](auto a,auto b){
                    lease.check_cancelled();return prepared_query_detail::compare(c,*prepared_query_detail::element(c,a),*prepared_query_detail::element(c,b))<0;
                });++plan->indexed_projections;
            }
            // Choose a selective exact range, retaining nullable rows because
            // Unknown cannot be dropped from a three-valued conjunction.
            for(std::size_t clause=0;clause<plan->clauses.size();++clause) {
                const auto& c=plan->projections[plan->clauses[clause]];const auto& predicate=request.all_of[clause];
                if(c.scalar.kind!=composite::DeclarationKind::Integer && c.scalar.kind!=composite::DeclarationKind::Rational)continue;
                auto first=c.ordered.begin(),last=c.ordered.end(),lo=first,hi=last;
                const auto op=predicate.operation;
                if(op!=ExactPredicateKind::IsNull && op!=ExactPredicateKind::IsNotNull) {
                    lo=std::lower_bound(first,last,predicate.literal,[&](auto row,const auto& literal){return prepared_query_detail::compare(c,*prepared_query_detail::element(c,row),literal)<0;});
                    hi=std::upper_bound(lo,last,predicate.literal,[&](const auto& literal,auto row){return prepared_query_detail::compare(c,literal,*prepared_query_detail::element(c,row))<0;});
                }
                std::vector<std::size_t> selected;
                const auto add=[&](auto a,auto b){selected.insert(selected.end(),a,b);};
                switch(op) {
                    case ExactPredicateKind::Equal:add(lo,hi);break;
                    case ExactPredicateKind::NotEqual:add(first,lo);add(hi,last);break;
                    case ExactPredicateKind::Less:add(first,lo);break;
                    case ExactPredicateKind::LessEqual:add(first,hi);break;
                    case ExactPredicateKind::Greater:add(hi,last);break;
                    case ExactPredicateKind::GreaterEqual:add(lo,last);break;
                    case ExactPredicateKind::IsNull:break;
                    case ExactPredicateKind::IsNotNull:add(first,last);break;
                }
                if(op!=ExactPredicateKind::IsNotNull)add(c.null_rows.begin(),c.null_rows.end());
                if(selected.size()<rows && (!plan->candidates || selected.size()<plan->candidates->size())) {
                    std::sort(selected.begin(),selected.end());plan->candidates=std::move(selected);
                }
                lease.check_cancelled();
            }
            plan->tiles=machine::ceil_div(plan->candidates?plan->candidates->size():rows,rows_per_tile);
        }
        // A conservative bound covers numerator/denominator growth for every
        // possible selected sum and any exact tile merge, independent of order.
        if (request.sum && rows) required.integer_bits=machine::checked_size_mul(2,
            machine::checked_size_add(1,machine::checked_size_mul(rows,machine::checked_size_add(required.integer_bits,1))));
        runtime::admit_execution(required,lease.policy());lease.check_cancelled();
        plan->requirements=required;return PreparedNativeQuery(std::move(plan));
    }
    std::size_t row_count() const {return plan().source.assertions(plan().kind).size();}
    std::size_t projection_count() const {return plan().projections.size();}
    std::size_t payload_resolutions() const {return plan().payload_resolutions;}
    std::size_t candidate_row_count() const {return plan().candidates?plan().candidates->size():row_count();}
    std::size_t indexed_projection_count() const {return plan().indexed_projections;}
    const NativeQueryRequest& request() const & {return plan().request;}
    const NativeQueryRequest& request() const &&=delete;
    runtime::ExecutionRequirements requirements() const {return plan().requirements;}
    std::shared_ptr<const storage::TemporalEndpoint> source_publication() const {return plan().source.endpoint();}
    std::uint64_t source_namespace() const {return plan().source.schema().source_namespace;}
    std::size_t result_object_bytes() const {
        return machine::checked_size_add(machine::checked_size_add(sizeof(DerivedNativeQuery),sizeof(prepared_query_detail::Data)),
            machine::checked_size_add(request().output==NativeQueryOutput::Keys?machine::checked_size_mul(row_count(),sizeof(std::uint64_t)):0,
                request().sum?numeric::rational_limb_envelope(requirements().integer_bits):0));
    }
    std::size_t execution_object_bytes() const {
        return machine::checked_size_add(result_object_bytes(),machine::checked_size_add(
            machine::checked_size_mul(plan().tiles,machine::checked_size_add(sizeof(prepared_query_detail::Data),
                request().sum?numeric::rational_limb_envelope(requirements().integer_bits):0)),
            request().output==NativeQueryOutput::Keys?candidate_row_count():0));
    }
    DerivedNativeQuery execute(runtime::WorkAvailability availability,runtime::CpuTileExecutor& executor,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        const auto& p=plan();lease.check_cancelled();
        runtime::admit_work_availability(source_namespace(),p.source.sequence(),availability);
        runtime::admit_execution(p.requirements,lease.policy());
        const auto& keys=p.source.assertions(p.kind);
        std::vector<prepared_query_detail::Data> partials(p.tiles);
        const auto candidates=candidate_row_count();
        std::vector<std::uint8_t> states(request().output==NativeQueryOutput::Keys?candidates:0);
        prepared_query_detail::Data data;
        data.cpu=executor.run_tiles(p.tiles,[&](std::size_t tile,std::size_t,const runtime::CpuTileStop& stop) {
            auto& part=partials[tile];
            const auto begin=machine::checked_size_mul(tile,p.rows_per_tile);
            const auto end=begin+std::min(p.rows_per_tile,candidates-begin);
            for (auto slot=begin;slot<end;++slot) {
                const auto row=p.candidates?(*p.candidates)[slot]:slot;
                lease.check_cancelled();stop.check();auto truth=NativeTruth::True;
                for (std::size_t i=0;i<p.clauses.size();++i) {
                    const auto& c=p.projections[p.clauses[i]];++part.predicate_evaluations;
                    const auto term=native_query_detail::evaluate(c.scalar,p.request.all_of[i],*c.values[row]);
                    if (term==NativeTruth::False) {truth=term;break;}
                    if (term==NativeTruth::Unknown) truth=term;
                }
                if (!states.empty()) states[slot]=static_cast<std::uint8_t>(truth);
                if (truth==NativeTruth::Unknown) ++part.unknown;
                if (truth!=NativeTruth::True) continue;
                ++part.selected;
                if (!p.sum_column) continue;
                const auto& c=p.projections[*p.sum_column];const auto* value=c.values[row].get();
                if (c.scalar.nullable) {
                    const auto& option=std::get<composite::DeclaredValue::Optional>(value->data);
                    if (option.element.empty()) continue;
                    value=&option.element.front();
                }
                if (!part.sum) part.sum=numeric::Rational{0};
                if (c.scalar.kind==composite::DeclarationKind::Integer) *part.sum+=std::get<numeric::Integer>(value->data);
                else *part.sum+=std::get<numeric::Rational>(value->data);
                ++part.numeric_contributions;prepared_query_detail::check_bits(*part.sum,lease);
            }
        });
        for (const auto& part:partials) {
            lease.check_cancelled();data.selected+=part.selected;data.unknown+=part.unknown;
            data.numeric_contributions+=part.numeric_contributions;data.predicate_evaluations+=part.predicate_evaluations;
            if (part.sum) {if (!data.sum) data.sum=numeric::Rational{0};*data.sum+=*part.sum;prepared_query_detail::check_bits(*data.sum,lease);}
        }
        if (request().output==NativeQueryOutput::Keys) {
            data.keys.reserve(data.selected);
            for (std::size_t i=0;i<candidates;++i) {
                if (i%256==0) lease.check_cancelled();
                if (states[i]==static_cast<std::uint8_t>(NativeTruth::True)) data.keys.push_back(keys[p.candidates?(*p.candidates)[i]:i]);
            }
        }
        lease.check_cancelled();return DerivedNativeQuery(plan_,std::move(data),availability,lease.policy());
    }
private:
    explicit PreparedNativeQuery(std::shared_ptr<const prepared_query_detail::Plan> p):plan_(std::move(p)){}
    const prepared_query_detail::Plan& plan() const {if (!plan_) throw std::invalid_argument("moved native query");return *plan_;}
    std::shared_ptr<const prepared_query_detail::Plan> plan_;
    friend class NativeQueryCache;
};

// Each cache is bound to one complete immutable operation. Reuse across a new
// publication requires equality of all declared query dependencies, never just
// equality of the current answer or a hash. Ordered/repeated clauses are checked.
class NativeQueryCache {
public:
    explicit NativeQueryCache(std::size_t retained_byte_limit):limit_(retained_byte_limit){}
    DerivedNativeQuery execute(const PreparedNativeQuery& operation,runtime::WorkAvailability availability,
        runtime::CpuTileExecutor& executor,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        lease.check_cancelled();runtime::admit_work_availability(operation.source_namespace(),operation.source_publication()->sequence,availability);
        runtime::admit_execution(operation.requirements(),lease.policy());
        std::lock_guard lock(mutex_);lease.check_cancelled();
        if(cached_ && cached_->source_publication()->sequence<=availability.max_sequence &&
            same_dependencies(*cached_->plan_,operation.plan(),lease)) {
            auto result=*cached_;result.plan_=operation.plan_;result.availability_=availability;
            result.policy_=lease.policy();result.cache_hit_=true;++hits_;return result;
        }
        auto result=operation.execute(availability,executor,lease);++applications_;
        if(result.retained_object_bytes()<=limit_)cached_=result;
        else cached_.reset();
        return result;
    }
    void clear() {std::lock_guard lock(mutex_);cached_.reset();}
    std::pair<std::size_t,std::size_t> applications_hits() const {std::lock_guard lock(mutex_);return {applications_,hits_};}
private:
    static bool same_dependencies(const prepared_query_detail::Plan& a,const prepared_query_detail::Plan& b,const runtime::ExecutionLease& lease) {
        if(a.kind!=b.kind || !a.source.same_schema(b.source) || a.request.output!=b.request.output || a.request.sum!=b.request.sum ||
            a.request.all_of.size()!=b.request.all_of.size() || a.source.assertions(a.kind)!=b.source.assertions(b.kind) ||
            a.projections.size()!=b.projections.size() || a.source.lineage().assertions!=b.source.lineage().assertions)return false;
        for(std::size_t i=0;i<a.request.all_of.size();++i) {
            const auto& x=a.request.all_of[i];const auto& y=b.request.all_of[i];lease.check_cancelled();
            if(x.factors!=y.factors || x.operation!=y.operation || !composite::declared_value_equal(x.literal,y.literal))return false;
        }
        for(std::size_t column=0;column<a.projections.size();++column) {
            const auto& x=a.projections[column];const auto& y=b.projections[column];
            if(x.path!=y.path || x.scalar.selected!=y.scalar.selected || x.scalar.element!=y.scalar.element)return false;
            for(std::size_t row=0;row<x.values.size();++row) {
                lease.check_cancelled();if(!composite::declared_value_equal(*x.values[row],*y.values[row]))return false;
            }
        }
        return true;
    }
    std::size_t limit_,applications_{},hits_{};mutable std::mutex mutex_;
    std::optional<DerivedNativeQuery> cached_;
};
} // namespace rfc::database
