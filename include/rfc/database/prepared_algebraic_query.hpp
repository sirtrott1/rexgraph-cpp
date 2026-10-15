#pragma once
#include "rfc/database/algebraic_tensor_window.hpp"
#include "rfc/runtime/cpu_tiles.hpp"
#include <mutex>

namespace rfc::database {
enum class AlgebraicEquality { Equal, NotEqual };
struct AlgebraicCoordinatePredicate {
    std::vector<std::size_t> coordinate;
    AlgebraicEquality operation;
    numeric::AlgebraicElement literal;
};
struct AlgebraicQueryRequest {
    std::optional<AlgebraicRecordKind> kind;
    std::optional<local::AlgebraicModule> source_carrier,target_carrier;
    std::vector<AlgebraicCoordinatePredicate> all_of;
};
namespace algebraic_query_detail {
struct Plan {
    AlgebraicSnapshot source;
    AlgebraicQueryRequest request;
    std::vector<AlgebraicRecordPin> candidates;
    std::vector<std::vector<std::size_t>> coordinates;
    std::size_t rows_per_tile;
    runtime::ExecutionRequirements requirements;
};
} // namespace algebraic_query_detail
class PreparedAlgebraicQuery;
class AlgebraicQueryCache;
class DerivedAlgebraicQuery {
public:
    const std::vector<AlgebraicRecordPin>& records() const & { return *records_; }
    const std::vector<AlgebraicRecordPin>& records() const && = delete;
    std::shared_ptr<const std::vector<AlgebraicRecordPin>> records_pin() const { return records_; }
    const AlgebraicSnapshot& source() const & { return plan_->source; }
    const AlgebraicSnapshot& source() const && = delete;
    const AlgebraicQueryRequest& request() const & { return plan_->request; }
    const AlgebraicQueryRequest& request() const && = delete;
    runtime::WorkAvailability availability() const noexcept { return availability_; }
    runtime::CpuTileRunReport cpu_report() const { return cache_hit_?runtime::CpuTileRunReport{}:cpu_; }
    bool cache_hit() const noexcept { return cache_hit_; }
private:
    DerivedAlgebraicQuery(std::shared_ptr<const algebraic_query_detail::Plan> plan,std::vector<AlgebraicRecordPin> records,
        runtime::WorkAvailability availability,runtime::CpuTileRunReport cpu)
        :plan_(std::move(plan)),records_(std::make_shared<const std::vector<AlgebraicRecordPin>>(std::move(records))),availability_(availability),cpu_(cpu) {}
    std::shared_ptr<const algebraic_query_detail::Plan> plan_;
    std::shared_ptr<const std::vector<AlgebraicRecordPin>> records_;
    runtime::WorkAvailability availability_;
    runtime::CpuTileRunReport cpu_;
    bool cache_hit_{};
    friend class PreparedAlgebraicQuery;
    friend class AlgebraicQueryCache;
};

// Exact field equality uses the full coefficient tuple. No complex order,
// approximate predicate, coefficient pooling or inferred carrier equality.
class PreparedAlgebraicQuery {
public:
    static PreparedAlgebraicQuery prepare(const AlgebraicSnapshot& source,const AlgebraicQueryRequest& request,
        runtime::WorkAvailability availability,const runtime::ExecutionLease& lease=runtime::ExecutionLease{},std::size_t rows_per_tile=256) {
        lease.check_cancelled();runtime::admit_work_availability(source.source_namespace(),source.sequence(),availability);
        if (!rows_per_tile) throw std::invalid_argument("algebraic query tile size must be positive");
        if (request.kind && static_cast<unsigned>(*request.kind)>static_cast<unsigned>(AlgebraicRecordKind::Metric))
            throw std::invalid_argument("unknown algebraic record kind");
        const auto& bundle=source.archive().bundle();
        for (const auto* carrier:{&request.source_carrier,&request.target_carrier})
            if (*carrier && !bundle.stalk((*carrier)->cell()).same_owner(**carrier)) throw std::invalid_argument("query carrier owner differs");
        runtime::ExecutionRequirements required;
        const auto rows=source.archive().contents().records.size(),clauses=request.all_of.size(),degree=bundle.field().degree();
        required.records=machine::checked_size_add(machine::checked_size_mul(6,rows),machine::checked_size_mul(rows,clauses));
        required.records=machine::checked_size_add(required.records,clauses);
        required.rational_slots=machine::checked_size_mul(clauses,degree);
        required.scalar_work=machine::checked_size_mul(machine::checked_size_mul(rows,clauses),machine::checked_size_add(degree,2));
        runtime::admit_execution(required,lease.policy());
        for (const auto& clause:request.all_of) {
            if (clause.operation!=AlgebraicEquality::Equal && clause.operation!=AlgebraicEquality::NotEqual)
                throw std::invalid_argument("unknown algebraic equality predicate");
            if (!bundle.field().owns(clause.literal)) throw std::invalid_argument("algebraic query literal field owner differs");
            required.records=machine::checked_size_add(required.records,clause.coordinate.size());
            required.integer_bits=std::max(required.integer_bits,algebraic_database_detail::bits(clause.literal));
        }
        runtime::admit_execution(required,lease.policy());
        auto p=std::make_shared<algebraic_query_detail::Plan>(algebraic_query_detail::Plan{source,request,{},{},rows_per_tile,required});
        p->candidates.reserve(rows);p->coordinates.reserve(rows);
        for (const auto& record:source.archive().contents().records) {
            lease.check_cancelled();auto pin=source.record(record.key);
            if (request.kind && pin.kind()!=*request.kind) continue;
            if (request.source_carrier && !pin.source_carrier().same_owner(*request.source_carrier)) continue;
            if (request.target_carrier && !pin.target_carrier().same_owner(*request.target_carrier)) continue;
            std::vector<std::size_t> positions;positions.reserve(clauses);
            for (const auto& clause:request.all_of) {
                const auto index=pin.domain().flatten(clause.coordinate);
                p->requirements.integer_bits=std::max(p->requirements.integer_bits,algebraic_database_detail::bits(pin.coefficients()[index]));
                positions.push_back(index);
            }
            p->candidates.push_back(std::move(pin));p->coordinates.push_back(std::move(positions));
        }
        runtime::admit_execution(p->requirements,lease.policy());lease.check_cancelled();
        return PreparedAlgebraicQuery(std::move(p));
    }
    const AlgebraicSnapshot& source() const & { return plan().source; }
    const AlgebraicSnapshot& source() const && = delete;
    const AlgebraicQueryRequest& request() const & { return plan().request; }
    const AlgebraicQueryRequest& request() const && = delete;
    std::size_t candidate_count() const { return plan().candidates.size(); }
    runtime::ExecutionRequirements requirements() const { return plan().requirements; }
    DerivedAlgebraicQuery execute(runtime::WorkAvailability availability,runtime::CpuTileExecutor& executor,
        const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) const {
        lease.check_cancelled();runtime::admit_work_availability(source().source_namespace(),source().sequence(),availability);
        runtime::admit_execution(requirements(),lease.policy());
        const auto& p=plan();std::vector<unsigned char> selected(p.candidates.size());
        const auto tiles=machine::ceil_div(p.candidates.size(),p.rows_per_tile);
        const auto cpu=executor.run_tiles(tiles,[&](std::size_t tile,std::size_t,const runtime::CpuTileStop& stop) {
            const auto first=machine::checked_size_mul(tile,p.rows_per_tile),last=first+std::min(p.rows_per_tile,p.candidates.size()-first);
            for (auto row=first;row<last;++row) {
                lease.check_cancelled();stop.check();bool match=true;
                for (std::size_t i=0;i<p.request.all_of.size();++i) {
                    const auto& clause=p.request.all_of[i];
                    const bool equal=p.candidates[row].coefficients()[p.coordinates[row][i]]==clause.literal;
                    if (equal!=(clause.operation==AlgebraicEquality::Equal)) { match=false;break; }
                }
                selected[row]=static_cast<unsigned char>(match);
            }
        });
        std::vector<AlgebraicRecordPin> records;
        records.reserve(static_cast<std::size_t>(std::count(selected.begin(),selected.end(),1)));
        for (std::size_t i=0;i<selected.size();++i) { lease.check_cancelled();if (selected[i]) records.push_back(p.candidates[i]); }
        lease.check_cancelled();return DerivedAlgebraicQuery(plan_,std::move(records),availability,cpu);
    }
private:
    explicit PreparedAlgebraicQuery(std::shared_ptr<const algebraic_query_detail::Plan> p):plan_(std::move(p)) {}
    const algebraic_query_detail::Plan& plan() const { if (!plan_) throw std::invalid_argument("moved algebraic query");return *plan_; }
    std::shared_ptr<const algebraic_query_detail::Plan> plan_;
    friend class AlgebraicQueryCache;
};

// A result-pin count limit, reusable only for the same immutable prepared operation owner.
// New restores, requests and histories cannot hit by key, hash or equal output.
class AlgebraicQueryCache {
public:
    explicit AlgebraicQueryCache(std::size_t max_records):limit_(max_records) {}
    DerivedAlgebraicQuery execute(const PreparedAlgebraicQuery& operation,runtime::WorkAvailability availability,
        runtime::CpuTileExecutor& executor,const runtime::ExecutionLease& lease=runtime::ExecutionLease{}) {
        lease.check_cancelled();runtime::admit_work_availability(operation.source().source_namespace(),operation.source().sequence(),availability);
        runtime::admit_execution(operation.requirements(),lease.policy());
        std::lock_guard lock(mutex_);lease.check_cancelled();
        if (cached_ && cached_->plan_==operation.plan_) {
            auto result=*cached_;result.availability_=availability;result.cache_hit_=true;++hits_;return result;
        }
        auto result=operation.execute(availability,executor,lease);++applications_;
        if (result.records().size()<=limit_) cached_=result;else cached_.reset();
        return result;
    }
    void clear() { std::lock_guard lock(mutex_);cached_.reset(); }
    std::pair<std::size_t,std::size_t> applications_hits() const { std::lock_guard lock(mutex_);return {applications_,hits_}; }
private:
    std::size_t limit_,applications_{},hits_{};
    std::optional<DerivedAlgebraicQuery> cached_;
    mutable std::mutex mutex_;
};
} // namespace rfc::database
