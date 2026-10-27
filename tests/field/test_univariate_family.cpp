#include "rfc/geometry/univariate_family.hpp"
#include <iostream>
using namespace rfc;
using namespace geometry;
using Q=numeric::Rational;using P=numeric::RationalVector;using Guard=numeric::ExactActionGuard;
using Family=PreparedUnivariateFamily;using Expression=OwnedRationalExpression;
static std::size_t checks{};
static void require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class Error=std::invalid_argument,class F>static void rejects(F f,const char* why){bool bad=false;try{f();}catch(const Error&){bad=true;}require(bad,why);}
struct Entry {P numerator;P denominator{Q{1}};};
struct Case {const char* name;std::size_t rows,cols;Q lo{-3},hi{3};bool lc{true},hc{true};std::vector<P> guards;std::vector<Entry> entries;};
static std::vector<Case> cases(){return {
 {"pivot_switch",2,2,-2,2,true,true,{},{{{0,1}},{{1}},{{1}},{{0,1}}}},
 {"cancelled_hole",1,1,-2,2,true,true,{},{{{0,1},{0,1}}}},
 {"zero_with_hole",1,1,-2,2,true,true,{},{{{0},{0,1}}}},
 {"irrational_rank",2,2,-3,3,true,true,{},{{{-2,0,1}},{{0}},{{0}},{{1}}}},
 {"repeated_rank",1,1,-2,2,true,true,{},{{{1,-2,1}}}},
 {"endpoint_roots",1,1,-1,1,true,false,{},{{{-1,0,1}}}},
 {"mixed_guard",2,2,-3,3,true,true,{{-2,0,1}},{{{-1,0,1}},{{0}},{{0}},{{1}}}},
 {"rectangular_gcd",2,3,-2,2,true,true,{},{{{0,1}},{{1}},{{0}},{{1}},{{0,1}},{{1}}}},
 {"two_rank_drops",3,3,-2,2,true,true,{},{{{0,1}},{{0}},{{0}},{{0}},{{0,0,1}},{{0}},{{0}},{{0}},{{0,1}}}},
 {"denominator_clear",2,2,-3,3,true,true,{},{{{1},{-1,1}},{{0}},{{0}},{{-2,0,1},{1,1}}}},
 {"empty_guard",1,1,-2,2,true,true,{{0}},{{{1}}}},
 {"empty_interval",1,1,0,0,false,true,{},{{{1}}}},
 {"singleton_drop",1,1,1,1,true,true,{},{{{-1,1}}}},
 {"no_real_roots",1,1,-2,2,true,true,{},{{{1,0,1}}}},
 {"zero_rows",0,2,-2,2,true,true,{},{}},
 {"zero_columns",2,0,-2,2,true,true,{},{}},
 {"rank_one_rectangular",2,3,-3,3,true,true,{},{{{0,1}},{{0,0,1}},{{0,0,0,1}},{{0,2}},{{0,0,2}},{{0,0,0,2}}}},
 {"close_roots",1,1,0,1,true,true,{},{{{Q{1,1024},Q{-65,1024},Q{1}}}}},
 {"explicit_trailing_zero",1,1,-2,2,true,true,{{1,0,0}},{{{0,1,0},{1,0,0}}}},
 {"overlapping_rank_hole",1,1,-2,2,true,true,{{-1,1}},{{{-1,1}}}},
 {"mixed_reducible_roots",1,1,-3,3,true,true,{},{{{2,0,-3,0,1}}}},
 {"even_degree_no_real",1,1,-4,4,true,true,{},{{{4,0,5,0,1}}}},
 {"outside_rank_roots",1,1,-1,1,true,true,{},{{{-2,0,1}}}},
 {"endpoint_hole",1,1,0,1,true,true,{},{{{0,1},{0,1}}}},
 {"singleton_regular",1,1,2,2,true,true,{},{{{1}}}},
 {"two_irrational_rank_levels",3,3,-3,3,true,true,{},{{{-2,0,1}},{{0}},{{0}},{{0}},{{-2,0,1}},{{0}},{{0}},{{0}},{{-3,0,1}}}}
};}
static Family build(const Case& c,Guard& g){
 auto coordinate=UnivariateCoordinate::declare("t",7);auto domain=UnivariateDomain::declare(coordinate,c.lo,c.hi,c.lc,c.hc,c.guards,g);
 std::vector<Expression> entries;for(const auto& e:c.entries)entries.push_back(Expression::declare(domain,e.numerator,e.denominator,g));
 return Family::prepare(domain,UnivariateCarrier::declare("rows",c.rows,11),UnivariateCarrier::declare("columns",c.cols,13),entries,g);
}
static void qjson(const Q& q){std::cout<<'"'<<q<<'"';}
static void pjson(const P& p){std::cout<<'[';for(std::size_t i=0;i<p.size();++i){if(i)std::cout<<',';qjson(p[i]);}std::cout<<']';}
static void emit(const Case& c,const Family& f){
 std::cout<<"{\"name\":\""<<c.name<<"\",\"rows\":"<<c.rows<<",\"cols\":"<<c.cols<<",\"lower\":";qjson(c.lo);std::cout<<",\"upper\":";qjson(c.hi);
 std::cout<<",\"lower_closed\":"<<c.lc<<",\"upper_closed\":"<<c.hc<<",\"guards\":[";
 for(std::size_t i=0;i<c.guards.size();++i){if(i)std::cout<<',';pjson(c.guards[i]);}std::cout<<"],\"entries\":[";
 for(std::size_t i=0;i<c.entries.size();++i){if(i)std::cout<<',';std::cout<<'[';pjson(c.entries[i].numerator);std::cout<<',';pjson(c.entries[i].denominator);std::cout<<']';}
 std::cout<<"],\"generic_rank\":"<<f.generic_rank()<<",\"empty\":"<<f.empty()<<",\"rank_polynomial\":";pjson(f.rank_polynomial());std::cout<<",\"root_polynomial\":";pjson(f.root_polynomial());
 std::cout<<",\"points\":[";for(std::size_t i=0;i<f.points().size();++i){if(i)std::cout<<',';const auto& p=f.points()[i];std::cout<<"{\"interval\":[";qjson(p.interval.lower);std::cout<<',';qjson(p.interval.upper);std::cout<<"],\"endpoint\":"<<p.endpoint<<",\"hole\":"<<p.original_hole<<",\"admitted\":"<<p.admitted<<",\"rank\":";if(p.rank)std::cout<<*p.rank;else std::cout<<"null";std::cout<<'}';}
 std::cout<<"],\"cells\":[";for(std::size_t i=0;i<f.cells().size();++i){if(i)std::cout<<',';const auto& cell=f.cells()[i];std::cout<<"{\"left\":"<<cell.left_point<<",\"right\":"<<cell.right_point<<",\"sample\":";qjson(cell.rational_sample);std::cout<<",\"rank\":"<<cell.rank<<'}';}std::cout<<"]}";
}
int main(int argc,char** argv){try{
 const bool dump=argc==2&&std::string(argv[1])=="--dump";Guard g;const auto inputs=cases();if(dump)std::cout<<'[';
 for(std::size_t i=0;i<inputs.size();++i){const auto f=build(inputs[i],g);require(f.entries().size()==inputs[i].entries.size(),"original entry coverage");
  for(std::size_t j=0;j<f.entries().size();++j){require(f.entries()[j].original_numerator()==inputs[i].entries[j].numerator,"original numerator retained");require(f.entries()[j].original_denominator()==inputs[i].entries[j].denominator,"original denominator retained");}
  for(std::size_t j=0;j<f.points().size();++j){const auto p=f.point(j);if(p.record().admitted)require(f.rank_at(p,g)==*p.record().rank,"fresh algebraic rank specialization");else rejects<std::domain_error>([&]{(void)f.rank_at(p,g);},"excluded specialization refused");}
  for(const auto& cell:f.cells())require(f.rank_at(f.domain().at(cell.rational_sample,g),g)==cell.rank,"regular-cell rank");
  if(dump){if(i)std::cout<<',';emit(inputs[i],f);}
 }
 const auto pivot=build(inputs[0],g);require(pivot.generic_rank()==2&&pivot.points().size()==4,"exact plus/minus one census");
 require(pivot.rank_at(pivot.domain().at(Q{0},g),g)==2,"vanishing selected pivot is not rank loss");
 require(pivot.rank_at(pivot.domain().at(Q{1},g),g)==1,"positive exceptional rank");require(pivot.rank_at(pivot.domain().at(Q{-1},g),g)==1,"negative exceptional rank");
 const auto hole=build(inputs[1],g);require(hole.entries()[0].formal()==numeric::RationalFunction(Q{1}),"formal cancellation unchanged");
 rejects<std::domain_error>([&]{(void)hole.entries()[0].evaluate(hole.domain().at(Q{0},g),g);},"t/t original hole retained");
 require(hole.entries()[0].evaluate(hole.domain().at(Q{1},g),g)==1,"t/t admitted evaluation");
 const auto other=build(inputs[0],g);rejects([&]{(void)pivot.rank_at(other.point(1),g);},"foreign algebraic point");
 rejects([&]{(void)pivot.rank_at(other.domain().at(Q{0},g),g);},"foreign equal coordinate domain");
 const auto equal_domain=UnivariateDomain::declare(pivot.domain().coordinate(),-2,2,true,true,{},g);
 auto foreign_entry=Expression::declare(equal_domain,{1},{1},g);
 rejects([&]{(void)Family::prepare(pivot.domain(),UnivariateCarrier::declare("row",1),UnivariateCarrier::declare("column",1),{foreign_entry},g);},"same coordinate does not identify domains");
 require(!pivot.rows().same_owner(pivot.columns()),"equal dimensions not equal carriers");require(pivot.rows().revision()==11&&pivot.domain().coordinate().revision()==7,"revisions retained");
 rejects([&]{(void)Expression::declare(equal_domain,{1},{0},g);},"zero expression denominator");
 rejects([&]{(void)UnivariateDomain::declare(pivot.domain().coordinate(),2,-2,true,true,{},g);},"reversed interval");
 rejects([&]{(void)Family::prepare(equal_domain,UnivariateCarrier::declare("row",1),UnivariateCarrier::declare("col",1),{},g);},"missing full carrier entries");
 rejects<std::out_of_range>([&]{(void)pivot.point(99);},"out of range point");
 rejects<std::domain_error>([&]{(void)pivot.rank_at(pivot.domain().at(Q{3},g),g);},"outside bounded interval");
 auto retained=[](){Guard local;return build(cases()[0],local).point(1);}();require(retained.record().admitted,"point retains source lifetime");
 // Two explicit real rational phase charts: neither finite chart is a full
 // circle. The second includes h=-1 at u=0; u=1/s declares the overlap.
 const auto one=Expression::declare(equal_domain,{1},{1},g);const auto real=Expression::declare(equal_domain,{1,0,-1},{1,0,1},g),imag=Expression::declare(equal_domain,{0,2},{1,0,1},g);
 require(real.formal()*real.formal()+imag.formal()*imag.formal()==one.formal(),"unitarity is an exact rational-family identity");
 auto second_domain=UnivariateDomain::declare(UnivariateCoordinate::declare("u"),-2,2,true,true,{},g);
 const auto second_real=Expression::declare(second_domain,{-1,0,1},{1,0,1},g),second_imag=Expression::declare(second_domain,{0,2},{1,0,1},g);
 require(second_real.evaluate(second_domain.at(Q{0},g),g)==-1&&second_imag.evaluate(second_domain.at(Q{0},g),g)==0,"explicit second chart minus-one endpoint");
 for(const Q& at:P{Q{-2},Q{-1},Q{1,2},Q{1},Q{2}}){const Q inverse=Q{1}/at;require(real.evaluate(equal_domain.at(at,g),g)==second_real.evaluate(second_domain.at(inverse,g),g),"declared chart overlap real");require(imag.evaluate(equal_domain.at(at,g),g)==second_imag.evaluate(second_domain.at(inverse,g),g),"declared chart overlap imaginary");}
 // Receiving limits and cancellation cannot yield a completed certificate.
 auto limits=numeric::ExactActionLimits{};limits.dimension=1;Guard small_dim(limits);rejects<std::length_error>([&]{(void)Family::prepare(pivot.domain(),pivot.rows(),pivot.columns(),pivot.entries(),small_dim);},"receiving dimension");
 limits=numeric::ExactActionLimits{};limits.entries=0;Guard small_entries(limits);rejects<std::length_error>([&]{(void)Family::prepare(pivot.domain(),pivot.rows(),pivot.columns(),pivot.entries(),small_entries);},"receiving entries");
 limits=numeric::ExactActionLimits{};limits.steps=0;Guard small_work(limits);rejects<std::length_error>([&]{(void)Family::prepare(pivot.domain(),pivot.rows(),pivot.columns(),pivot.entries(),small_work);},"receiving work");
 limits=numeric::ExactActionLimits{};limits.integer_bits=1;Guard small_bits(limits);rejects<std::length_error>([&]{(void)Family::prepare(pivot.domain(),pivot.rows(),pivot.columns(),pivot.entries(),small_bits);},"receiving coefficient growth");
 UnivariateLimits profile;profile.max_minors=0;rejects<std::length_error>([&]{(void)Family::prepare(pivot.domain(),pivot.rows(),pivot.columns(),pivot.entries(),g,profile);},"complete minor census required");
 profile={};profile.max_bisections=0;rejects<std::length_error>([&]{(void)Family::prepare(pivot.domain(),pivot.rows(),pivot.columns(),pivot.entries(),g,profile);},"root isolation bound");
 profile={};profile.max_degree=0;rejects<std::length_error>([&]{(void)Family::prepare(pivot.domain(),pivot.rows(),pivot.columns(),pivot.entries(),g,profile);},"receiving polynomial degree");
 Guard measurement;const auto measured=Family::prepare(pivot.domain(),pivot.rows(),pivot.columns(),pivot.entries(),measurement);
 for(const auto cutoff:{std::size_t{1},measurement.steps()/2,measurement.steps()}){std::size_t steps{};Guard cancelled({},[&]{if(++steps==cutoff)throw std::runtime_error("cancelled");});rejects<std::runtime_error>([&]{(void)Family::prepare(pivot.domain(),pivot.rows(),pivot.columns(),pivot.entries(),cancelled);},"early/middle/final cancellation");}
 const auto wide=UnivariateDomain::declare(UnivariateCoordinate::declare("wide"),Q{100},Q{100},true,true,{},g);
 Guard weak_endpoint(numeric::ExactActionLimits{4096,16000000,64000000,2});
 rejects<std::length_error>([&]{(void)Expression::declare(wide,{1},{1},weak_endpoint);},"receiving endpoint bits");
 Guard tiny_point(numeric::ExactActionLimits{4096,0,64000000,100});
 rejects<std::length_error>([&]{(void)pivot.rank_at(pivot.point(1),tiny_point);},"point receiving retained budget");
 require(measured.work().minor_candidates==5,"all minors retained, not sampled rank");
 if(dump)std::cout<<"]\n";else std::cout<<checks<<" univariate domain, complete rank, exact root, ownership and resource checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
