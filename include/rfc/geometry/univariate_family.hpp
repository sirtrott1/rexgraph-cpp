#pragma once
#include "rfc/geometry/real_algebraic_embedding.hpp"
#include "rfc/numeric/rational_function.hpp"
#include "rfc/numeric/checked_exact_actions.hpp"
#include <bit>
#include <string>

namespace rfc::geometry {
struct UnivariateLimits {
    std::size_t max_dimension{8},max_degree{128},max_minors{10000},max_bisections{4096};
};
namespace univariate_detail {
using Q=numeric::Rational;using P=numeric::RationalVector;using Guard=numeric::ExactActionGuard;
// Native polynomial and Sturm primitives, with receiving bounds and cancellation
// checkpoints between bounded scalar phases. This is container/work accounting,
// not an estimate of the GMP allocator or process RSS.
struct Work {
    Guard& guard;UnivariateLimits limits;numeric::AlgebraicFieldLimits scalar_limits;
    numeric::algebraic_detail::Work scalar;std::size_t charged{},bisections{},minors{},retained{};
    Work(Guard& g,UnivariateLimits l):guard(g),limits(l),scalar_limits{l.max_degree,l.max_degree+1,
        g.limits().integer_bits,g.limits().steps-g.steps(),g.limits().entries},scalar{scalar_limits}{
        guard.step();if(l.max_dimension>16 || l.max_degree>=numeric::RationalFunction::max_coefficients)
            throw std::length_error("univariate admitted dimension/degree profile");
    }
    void sync(){while(charged<scalar.steps){guard.step();++charged;}guard.step();}
    void scratch(std::size_t n){guard.entries(machine::checked_size_add(retained,n));}
    void admit(const P& p){guard.step();scratch(p.size());if(p.size()>limits.max_degree+1)throw std::length_error("univariate polynomial degree");guard.vector(p);}
    void keep(std::size_t n){retained=machine::checked_size_add(retained,n);guard.entries(retained);}
    P input(const P& p){admit(p);auto a=numeric::algebraic_detail::input(p,scalar);sync();return a;}
    P mul(const P& a,const P& b){guard.step();if(!a.empty()&&!b.empty()&&a.size()-1>limits.max_degree-(b.size()-1))throw std::length_error("univariate product degree");scratch(machine::checked_size_mul(machine::checked_size_add(a.size(),b.size()),4));auto p=numeric::algebraic_detail::multiply(a,b,scalar);sync();return p;}
    P sub(P a,const P& b){scratch(machine::checked_size_mul(std::max(a.size(),b.size()),4));auto p=numeric::algebraic_detail::subtract(std::move(a),b,scalar);sync();return p;}
    std::pair<P,P> divide(P a,const P& b){scratch(machine::checked_size_mul(machine::checked_size_add(a.size(),b.size()),4));auto p=numeric::algebraic_detail::divide(std::move(a),b,scalar);sync();return p;}
    P monic(P a){if(!a.empty()){const auto lead=a.back();for(auto& q:a)q=scalar.checked(Q(q/lead));}sync();return a;}
    P gcd(P a,P b){while(!b.empty()){guard.step();auto r=divide(a,b).second;a=std::move(b);b=std::move(r);}return monic(std::move(a));}
    P derivative(const P& a){P p(a.empty()?0:a.size()-1);for(std::size_t i=1;i<a.size();++i)p[i-1]=scalar.checked(Q(a[i]*numeric::Integer(std::to_string(i))));sync();return p;}
    P squarefree(const P& a){if(a.size()<2)return P{Q{1}};return monic(divide(a,gcd(a,derivative(a))).first);}
    Q evaluate(const P& p,const Q& x){auto q=real_root_detail::evaluate(p,x,scalar);sync();return q;}
    std::vector<P> sturm(const P& p){if(p.size()<2)return {};scratch(machine::checked_size_mul(p.size(),p.size()+1));auto a=real_root_detail::sturm(p,scalar);sync();return a;}
    std::size_t variations(const std::vector<P>& st,const Q& x){auto n=real_root_detail::variations(st,x,scalar);sync();return n;}
    std::size_t count(const std::vector<P>& st,const Q& lo,const Q& hi){const auto a=variations(st,lo),b=variations(st,hi);if(a<b)throw std::logic_error("univariate Sturm monotonicity");return a-b;}
    Q split(const P& p,const Q& lo,const Q& hi){
        Q mid=scalar.checked(Q((lo+hi)/2));sync();
        while(evaluate(p,mid)==0){bisect();mid=scalar.checked(Q((lo+mid)/2));sync();}return mid;
    }
    void bisect(){guard.step();if(bisections==limits.max_bisections)throw std::length_error("univariate isolation budget");++bisections;}
    void refine(const P& p,const std::vector<P>& st,ExactRealInterval& x){bisect();const auto mid=split(p,x.lower,x.upper);if(count(st,x.lower,mid))x.upper=mid;else x.lower=mid;}
    void isolate(const P& p,const std::vector<P>& st,Q lo,Q hi,std::size_t n,std::vector<ExactRealInterval>& out){
        // Iterative work list avoids a call stack proportional to root separation.
        struct Task{Q lo,hi;std::size_t count;};std::vector<Task> tasks{{std::move(lo),std::move(hi),n}};
        while(!tasks.empty()){
            guard.step();auto t=std::move(tasks.back());tasks.pop_back();if(!t.count)continue;
            if(t.count==1){out.push_back({std::move(t.lo),std::move(t.hi)});continue;}
            bisect();const auto mid=split(p,t.lo,t.hi);const auto left=count(st,t.lo,mid);
            guard.entries(machine::checked_size_add(retained,machine::checked_size_mul(tasks.size()+2,3)));
            tasks.push_back({mid,t.hi,t.count-left});tasks.push_back({t.lo,mid,left});
        }
    }
    bool zero_at(const P& q,const P& roots,const ExactRealInterval& x){
        if(x.lower==x.upper)return evaluate(q,x.lower)==0;
        const auto common=gcd(roots,q);return common.size()>1 && count(sturm(common),x.lower,x.upper)==1;
    }
};
inline std::vector<std::vector<std::size_t>> subsets(std::size_t n,std::size_t k,Work& w){
    std::vector<std::vector<std::size_t>> out;
    for(std::size_t mask=0;mask<(std::size_t{1}<<n);++mask){w.guard.step();if(static_cast<std::size_t>(std::popcount(mask))!=k)continue;
        w.guard.entries(machine::checked_size_add(w.retained,machine::checked_size_mul(out.size()+1,k)));
        std::vector<std::size_t> indices;for(std::size_t i=0;i<n;++i)if(mask&(std::size_t{1}<<i))indices.push_back(i);out.push_back(std::move(indices));}
    return out;
}
inline P determinant(const std::vector<P>& a,std::size_t columns,const std::vector<std::size_t>& rows,const std::vector<std::size_t>& cols,Work& w){
    const auto count=std::size_t{1}<<rows.size();
    w.guard.entries(machine::checked_size_add(w.retained,machine::checked_size_mul(count,w.limits.max_degree+1)));
    std::vector<P> dp(count);dp[0]={Q{1}};
    for(std::size_t mask=1;mask<count;++mask){const auto row=static_cast<std::size_t>(std::popcount(mask))-1;std::size_t position{};
        for(std::size_t j=0;j<cols.size();++j)if(mask&(std::size_t{1}<<j)){
            auto term=w.mul(dp[mask^(std::size_t{1}<<j)],a[rows[row]*columns+cols[j]]);
            if((row+position)%2==0)for(auto& q:term)q=-q;
            dp[mask]=w.sub(std::move(dp[mask]),term);++position;}}
    return dp.back();
}
} // namespace univariate_detail

class UnivariateCoordinate {
    struct State{std::string name;std::uint64_t revision;};
public:
    static UnivariateCoordinate declare(std::string name,std::uint64_t revision=0){if(name.empty())throw std::invalid_argument("empty parameter name");return UnivariateCoordinate(std::make_shared<const State>(State{std::move(name),revision}));}
    bool same_owner(const UnivariateCoordinate& x)const noexcept{return state_==x.state_;}
    const std::string& name()const & noexcept{return state_->name;}const std::string& name()const &&=delete;
    template<std::floating_point T>static UnivariateCoordinate declare(std::string,T)=delete;
    std::uint64_t revision()const noexcept{return state_->revision;}
private:
    explicit UnivariateCoordinate(std::shared_ptr<const State> s):state_(std::move(s)){}std::shared_ptr<const State> state_;
};
class UnivariateDomain {
    using Q=univariate_detail::Q;using P=univariate_detail::P;using Guard=univariate_detail::Guard;
    struct State{UnivariateCoordinate coordinate;Q lower,upper;bool lower_closed,upper_closed;std::vector<P> guards;};
public:
    class Value {
        friend class UnivariateDomain;
        Value(std::shared_ptr<const State> s,Q q):state_(std::move(s)),value_(std::move(q)){}
        std::shared_ptr<const State> state_;Q value_;
    public:
        const Q& value()const & noexcept{return value_;}const Q& value()const &&=delete;
    };
    static UnivariateDomain declare(UnivariateCoordinate coordinate,Q lower,Q upper,bool lower_closed,bool upper_closed,
        std::vector<P> original_guards,Guard& guard,UnivariateLimits limits={}){
        univariate_detail::Work work(guard,limits);guard.value(lower);guard.value(upper);lower=numeric::canonical_rational(lower);upper=numeric::canonical_rational(upper);
        if(lower>upper)throw std::invalid_argument("reversed parameter interval");
        guard.entries(original_guards.size());std::size_t retained=2;
        for(auto& p:original_guards){work.admit(p);retained=machine::checked_size_add(retained,p.size());guard.entries(retained);p=numeric::canonical_vector(p);}
        return UnivariateDomain(std::make_shared<const State>(State{std::move(coordinate),std::move(lower),std::move(upper),lower_closed,upper_closed,std::move(original_guards)}));
    }
    template<class A,class B>requires(std::floating_point<A>||std::floating_point<B>)
    static UnivariateDomain declare(UnivariateCoordinate,A,B,bool,bool,std::vector<P>,Guard&,UnivariateLimits={})=delete;
    bool same_owner(const UnivariateDomain& d)const noexcept{return state_==d.state_;}
    bool owns(const Value& v)const noexcept{return state_==v.state_;}
    Value at(Q q,Guard& guard)const{guard.step();guard.value(q);q=numeric::canonical_rational(q);return Value(state_,std::move(q));}
    template<std::floating_point T>Value at(T,Guard&)const=delete;
    const UnivariateCoordinate& coordinate()const & noexcept{return state_->coordinate;}const UnivariateCoordinate& coordinate()const &&=delete;
    const Q& lower()const & noexcept{return state_->lower;}const Q& lower()const &&=delete;
    const Q& upper()const & noexcept{return state_->upper;}const Q& upper()const &&=delete;
    bool lower_closed()const noexcept{return state_->lower_closed;}bool upper_closed()const noexcept{return state_->upper_closed;}
    const std::vector<P>& original_guards()const & noexcept{return state_->guards;}const std::vector<P>& original_guards()const &&=delete;
    void validate(Guard& guard,UnivariateLimits limits={})const{
        univariate_detail::Work w(guard,limits);guard.value(lower());guard.value(upper());
        for(const auto& p:original_guards()){(void)w.input(p);w.keep(p.size()+1);}
    }
    bool contains(const Value& value,Guard& guard,UnivariateLimits limits={})const{
        if(!owns(value))throw std::invalid_argument("foreign parameter domain owner");
        univariate_detail::Work w(guard,limits);const auto& q=value.value();guard.value(q);guard.value(lower());guard.value(upper());
        if(q<lower()||q>upper()||(q==lower()&&!lower_closed())||(q==upper()&&!upper_closed()))return false;
        for(const auto& p:original_guards())if(w.evaluate(w.input(p),q)==0)return false;
        return true;
    }
private:
    explicit UnivariateDomain(std::shared_ptr<const State> s):state_(std::move(s)){}std::shared_ptr<const State> state_;
};
// Each entry is an original declaration, not just an equivalence class in Q(t).
// Repeated and identically-zero entries retain their denominators as well.
class OwnedRationalExpression {
    using P=univariate_detail::P;using Guard=univariate_detail::Guard;
    struct State{UnivariateDomain domain;P numerator,denominator;numeric::RationalFunction formal;};
public:
    static OwnedRationalExpression declare(UnivariateDomain domain,P numerator,P denominator,Guard& guard,UnivariateLimits limits={}){
        domain.validate(guard,limits);univariate_detail::Work w(guard,limits);auto n=w.input(numerator),d=w.input(denominator);if(d.empty())throw std::invalid_argument("identically zero expression denominator");
        w.keep(machine::checked_size_add(numerator.size(),denominator.size()));
        const auto common=w.gcd(n,d);auto nn=w.divide(n,common).first,dd=w.divide(d,common).first;
        // The native Q(t) constructor remains the formal bridge; it does not
        // replace the retained declaration or its original denominator guard.
        numeric::RationalFunction formal(std::move(nn),std::move(dd));
        w.admit(formal.numerator());w.admit(formal.denominator());guard.step();
        return OwnedRationalExpression(std::make_shared<const State>(State{std::move(domain),numeric::canonical_vector(numerator),numeric::canonical_vector(denominator),std::move(formal)}));
    }
    const UnivariateDomain& domain()const & noexcept{return state_->domain;}const UnivariateDomain& domain()const &&=delete;
    const P& original_numerator()const & noexcept{return state_->numerator;}const P& original_numerator()const &&=delete;
    const P& original_denominator()const & noexcept{return state_->denominator;}const P& original_denominator()const &&=delete;
    const numeric::RationalFunction& formal()const & noexcept{return state_->formal;}const numeric::RationalFunction& formal()const &&=delete;
    numeric::Rational evaluate(const UnivariateDomain::Value& at,Guard& guard,UnivariateLimits limits={})const{
        if(!domain().contains(at,guard,limits))throw std::domain_error("parameter outside original domain");
        univariate_detail::Work w(guard,limits);
        const auto d=w.evaluate(w.input(original_denominator()),at.value());if(d==0)throw std::domain_error("original denominator hole");
        return guard.div(w.evaluate(w.input(original_numerator()),at.value()),d);
    }
private:
    explicit OwnedRationalExpression(std::shared_ptr<const State> s):state_(std::move(s)){}std::shared_ptr<const State> state_;
};
class UnivariateCarrier {
    struct State{std::string name;std::size_t dimension;std::uint64_t revision;};
public:
    static UnivariateCarrier declare(std::string name,std::size_t dimension,std::uint64_t revision=0){if(name.empty())throw std::invalid_argument("empty carrier name");return UnivariateCarrier(std::make_shared<const State>(State{std::move(name),dimension,revision}));}
    template<class A,class B=std::uint64_t>requires(std::floating_point<A>||std::floating_point<B>)
    static UnivariateCarrier declare(std::string,A,B=0)=delete;
    bool same_owner(const UnivariateCarrier& c)const noexcept{return state_==c.state_;}
    std::size_t dimension()const noexcept{return state_->dimension;}std::uint64_t revision()const noexcept{return state_->revision;}
    const std::string& name()const & noexcept{return state_->name;}const std::string& name()const &&=delete;
private:
    explicit UnivariateCarrier(std::shared_ptr<const State> s):state_(std::move(s)){}std::shared_ptr<const State> state_;
};

// Complete finite rank stratification on a declared bounded interval. No pivot
// exclusion is introduced: the common zeros of ALL maximal minors are exactly
// the rank exceptions. Smaller minors certify every exceptional specialization.
class PreparedUnivariateFamily {
    using P=univariate_detail::P;using Q=univariate_detail::Q;using Guard=univariate_detail::Guard;
public:
    struct PointRecord {
        ExactRealInterval interval;bool endpoint{},original_hole{},admitted{};std::optional<std::size_t> rank;
    };
    struct Cell {std::size_t left_point,right_point;Q rational_sample;std::size_t rank;};
    struct WorkReceipt {std::size_t original_entries,original_coefficients,minor_candidates,retained_coefficient_allowance,scalar_steps,bisections;};
private:
    struct State {
        UnivariateDomain domain;UnivariateCarrier rows,columns;std::vector<OwnedRationalExpression> entries;
        P clearing_denominator,rank_polynomial,root_polynomial;std::vector<std::vector<P>> minors;
        std::vector<P> guards;std::vector<PointRecord> points;std::vector<Cell> cells;std::size_t generic_rank;WorkReceipt work;
    };
public:
    class Point {
        friend class PreparedUnivariateFamily;
        Point(std::shared_ptr<const State> s,std::size_t i):state_(std::move(s)),index_(i){}
        std::shared_ptr<const State> state_;std::size_t index_;
    public:
        const PointRecord& record()const & noexcept{return state_->points[index_];}const PointRecord& record()const &&=delete;
    };
    static PreparedUnivariateFamily prepare(UnivariateDomain domain,UnivariateCarrier rows,UnivariateCarrier columns,
        std::vector<OwnedRationalExpression> entries,Guard& guard,UnivariateLimits limits={}){
        using namespace univariate_detail;const auto start=guard.steps();domain.validate(guard,limits);Work w(guard,limits);
        const auto m=rows.dimension(),n=columns.dimension(),count=machine::checked_size_mul(m,n);
        guard.dimension(m);guard.dimension(n);if(m>limits.max_dimension||n>limits.max_dimension)throw std::length_error("univariate matrix dimension");
        guard.entries(count);if(entries.size()!=count)throw std::invalid_argument("univariate full carrier entries required");
        std::vector<P> guards;std::size_t original_coefficients{};P denominator{Q{1}};
        const auto retain_guard=[&](const P& p){auto a=w.input(p);w.keep(machine::checked_size_mul(p.size()+1,2));original_coefficients=machine::checked_size_add(original_coefficients,p.size());guards.push_back(std::move(a));};
        for(const auto& p:domain.original_guards())retain_guard(p);
        for(const auto& entry:entries){
            guard.step();if(!domain.same_owner(entry.domain()))throw std::invalid_argument("foreign expression/domain owner");
            (void)w.input(entry.original_numerator());retain_guard(entry.original_denominator());
            original_coefficients=machine::checked_size_add(original_coefficients,entry.original_numerator().size());w.keep(entry.original_numerator().size()+1);
            w.admit(entry.formal().numerator());w.admit(entry.formal().denominator());
            w.keep(machine::checked_size_mul(entry.formal().coefficient_count(),2));
            denominator=w.mul(denominator,entry.formal().denominator());
        }
        std::vector<P> polynomial_matrix;polynomial_matrix.reserve(count);
        for(const auto& entry:entries){auto division=w.divide(denominator,entry.formal().denominator());if(!division.second.empty())throw std::logic_error("univariate common denominator residual");
            auto p=w.mul(entry.formal().numerator(),division.first);w.keep(p.size()+1);polynomial_matrix.push_back(std::move(p));}
        const auto max_rank=std::min(m,n);std::vector<std::vector<P>> all_minors(max_rank+1);std::size_t generic_rank{};P rank_polynomial{Q{1}};
        for(std::size_t k=1;k<=max_rank;++k){auto rr=subsets(m,k,w),cc=subsets(n,k,w);P common;
            const auto candidates=machine::checked_size_mul(rr.size(),cc.size());if(candidates>limits.max_minors-w.minors)throw std::length_error("univariate complete minor budget");
            w.guard.entries(machine::checked_size_add(w.retained,candidates));
            for(const auto& ri:rr)for(const auto& ci:cc){guard.step();++w.minors;auto p=determinant(polynomial_matrix,n,ri,ci,w);common=w.gcd(common,p);w.keep(p.size()+1);all_minors[k].push_back(std::move(p));}
            if(!common.empty()){generic_rank=k;rank_polynomial=w.squarefree(common);}
        }
        P root_polynomial=rank_polynomial;bool empty=domain.lower()==domain.upper()&&!(domain.lower_closed()&&domain.upper_closed());
        for(const auto& p:guards){if(p.empty()){empty=true;continue;}root_polynomial=w.squarefree(w.mul(root_polynomial,w.squarefree(p)));}
        w.keep(machine::checked_size_add(denominator.size(),machine::checked_size_add(rank_polynomial.size(),root_polynomial.size())));
        std::vector<PointRecord> points;std::vector<Cell> cells;
        if(!empty){
            // Strip rational endpoint factors before Sturm isolation; endpoints
            // are recorded independently, including open endpoints and holes.
            P interior=root_polynomial;const auto lo=domain.lower(),hi=domain.upper();
            if(w.evaluate(interior,lo)==0)interior=w.divide(interior,P{-lo,Q{1}}).first;
            if(hi!=lo&&w.evaluate(interior,hi)==0)interior=w.divide(interior,P{-hi,Q{1}}).first;
            std::vector<ExactRealInterval> roots;const auto st=w.sturm(interior);
            if(lo<hi && interior.size()>1)w.isolate(interior,st,lo,hi,w.count(st,lo,hi),roots);
            for(std::size_t i=0;i<roots.size();++i){while(roots[i].lower<=lo||roots[i].upper>=hi||(i&&roots[i-1].upper>=roots[i].lower))w.refine(interior,st,roots[i]);}
            points.push_back({{lo,lo},true,false,false,{}});for(auto& x:roots)points.push_back({std::move(x),false,false,false,{}});
            if(hi!=lo)points.push_back({{hi,hi},true,false,false,{}});
            for(auto& point:points){guard.step();for(const auto& p:guards)if(w.zero_at(p,root_polynomial,point.interval))point.original_hole=true;
                point.admitted=!point.original_hole && (!point.endpoint || (point.interval.lower==lo?domain.lower_closed():domain.upper_closed()));
                if(lo==hi)point.admitted=point.admitted&&domain.upper_closed();
                if(point.admitted){std::size_t rank{};for(std::size_t k=1;k<all_minors.size();++k)for(const auto& p:all_minors[k])if(!w.zero_at(p,root_polynomial,point.interval)){rank=k;break;}point.rank=rank;}}
            for(std::size_t i=1;i<points.size();++i){const Q sample=guard.div(guard.add(points[i-1].interval.upper,points[i].interval.lower),Q{2});
                if(!(points[i-1].interval.upper<sample&&sample<points[i].interval.lower))throw std::logic_error("univariate cell separation");
                cells.push_back({i-1,i,sample,generic_rank});}
        }
        w.keep(machine::checked_size_add(machine::checked_size_mul(points.size(),8),machine::checked_size_mul(cells.size(),4)));
        guard.step();WorkReceipt receipt{count,original_coefficients,w.minors,w.retained,guard.steps()-start,w.bisections};
        return PreparedUnivariateFamily(std::make_shared<const State>(State{std::move(domain),std::move(rows),std::move(columns),std::move(entries),std::move(denominator),std::move(rank_polynomial),std::move(root_polynomial),std::move(all_minors),std::move(guards),std::move(points),std::move(cells),generic_rank,receipt}));
    }
    const UnivariateDomain& domain()const & noexcept{return state_->domain;}const UnivariateDomain& domain()const &&=delete;
    const UnivariateCarrier& rows()const & noexcept{return state_->rows;}const UnivariateCarrier& rows()const &&=delete;
    const UnivariateCarrier& columns()const & noexcept{return state_->columns;}const UnivariateCarrier& columns()const &&=delete;
    const std::vector<OwnedRationalExpression>& entries()const & noexcept{return state_->entries;}const std::vector<OwnedRationalExpression>& entries()const &&=delete;
    const P& rank_polynomial()const & noexcept{return state_->rank_polynomial;}const P& rank_polynomial()const &&=delete;
    const P& root_polynomial()const & noexcept{return state_->root_polynomial;}const P& root_polynomial()const &&=delete;
    const P& clearing_denominator()const & noexcept{return state_->clearing_denominator;}const P& clearing_denominator()const &&=delete;
    const std::vector<PointRecord>& points()const & noexcept{return state_->points;}const std::vector<PointRecord>& points()const &&=delete;
    const std::vector<std::vector<P>>& minor_conditions()const & noexcept{return state_->minors;}const std::vector<std::vector<P>>& minor_conditions()const &&=delete;
    const std::vector<Cell>& cells()const & noexcept{return state_->cells;}const std::vector<Cell>& cells()const &&=delete;
    const WorkReceipt& work()const & noexcept{return state_->work;}const WorkReceipt& work()const &&=delete;
    std::size_t generic_rank()const noexcept{return state_->generic_rank;}
    bool empty()const noexcept{return state_->cells.empty()&&std::none_of(state_->points.begin(),state_->points.end(),[](const auto& p){return p.admitted;});}
    bool same_owner(const PreparedUnivariateFamily& f)const noexcept{return state_==f.state_;}
    Point point(std::size_t index)const{if(index>=state_->points.size())throw std::out_of_range("univariate point index");return Point(state_,index);}
    std::size_t rank_at(const Point& point,Guard& guard,UnivariateLimits limits={})const{
        if(state_!=point.state_)throw std::invalid_argument("foreign family specialization owner");
        univariate_detail::Work w(guard,limits);guard.entries(state_->work.retained_coefficient_allowance);guard.dimension(rows().dimension());guard.dimension(columns().dimension());
        if(rows().dimension()>limits.max_dimension||columns().dimension()>limits.max_dimension)throw std::length_error("receiving univariate dimension");
        w.admit(state_->root_polynomial);
        const auto& p=point.record();guard.value(p.interval.lower);guard.value(p.interval.upper);if(!p.admitted)throw std::domain_error("excluded exceptional specialization");
        std::size_t rank{};for(std::size_t k=1;k<state_->minors.size();++k)for(const auto& minor:state_->minors[k]){w.admit(minor);if(!w.zero_at(minor,state_->root_polynomial,p.interval)){rank=k;break;}}guard.step();return rank;
    }
    std::size_t rank_at(const UnivariateDomain::Value& at,Guard& guard,UnivariateLimits limits={})const{
        if(!domain().contains(at,guard,limits))throw std::domain_error("parameter outside original domain");
        univariate_detail::Work w(guard,limits);guard.entries(state_->work.retained_coefficient_allowance);guard.dimension(rows().dimension());guard.dimension(columns().dimension());
        if(rows().dimension()>limits.max_dimension||columns().dimension()>limits.max_dimension)throw std::length_error("receiving univariate dimension");
        w.admit(state_->root_polynomial);
        for(const auto& p:state_->guards)if(w.evaluate(w.input(p),at.value())==0)throw std::domain_error("original expression hole");
        std::size_t rank{};for(std::size_t k=1;k<state_->minors.size();++k)for(const auto& p:state_->minors[k])if(w.evaluate(w.input(p),at.value())!=0){rank=k;break;}guard.step();return rank;
    }
private:
    explicit PreparedUnivariateFamily(std::shared_ptr<const State> s):state_(std::move(s)){}std::shared_ptr<const State> state_;
};
} // namespace rfc::geometry
