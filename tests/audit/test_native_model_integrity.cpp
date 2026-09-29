#include "rfc/core/builder.hpp"
#include "rfc/learning/integer_network.hpp"
#include "rfc/learning/tower_network.hpp"
#include "rfc/learning/exact_metric_channels.hpp"
#include "rfc/learning/branching_layer.hpp"
#include "rfc/local/cellular_sheaf.hpp"
#include <array>
#include <bit>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <type_traits>
using namespace rfc;
namespace {
template<class T> concept AcceptsTemporaryLayer = requires(T&& owner) {
 learning::CompiledIntegerRelationalLayer(std::forward<T>(owner),core::Grade{1},{},{});
};
template<class T> concept AcceptsTemporaryMetricLayer = requires(T&& owner) {
 learning::CompiledExactMetricLayer(std::forward<T>(owner),{},{});
};
template<class T> concept AcceptsTemporaryCompiler = requires(T&& owner) {
 learning::compile_integer_relational_layer(std::forward<T>(owner),core::Grade{1},{},{});
};
static_assert(!AcceptsTemporaryLayer<core::RelationalComplex>);
static_assert(AcceptsTemporaryLayer<core::RelationalComplex&>);
static_assert(!AcceptsTemporaryMetricLayer<learning::ExactMetricChannelContext>);
static_assert(!AcceptsTemporaryCompiler<core::RelationalComplex>);
std::size_t checks = 0;
void check(bool x, const char* message) { ++checks; if (!x) throw std::runtime_error(message); }
template<class F> void rejects(F f, const char* message) { bool bad = false; try { f(); } catch(const std::exception&) { bad = true; } check(bad,message); }
core::RelationalComplex source() {
 core::RelationSignature s;auto t=s.add_type("edge",core::BoundaryPolicy::DeclaredExact,2,2);
 core::RelationalComplexBuilder b(s);b.add_grade_one(t,{{0,true,true,false,1},{1,true,false,false,1}});
 b.add_grade_one(t,{{1,true,true,false,1},{2,true,false,false,1}});return b.freeze();
}
void tags_and_types() {
 auto c=source(),other=c;const core::Grade g{1};
 field::MachineLatticeField x(c,g,{1,2});field::ExactLatticeField ex(c,g,{1,2});field::MachineLatticeTowerField tx(c);
 std::vector<field::ExactMetric> metrics{field::ExactMetric(numeric::ExactMatrix::identity(3)),field::ExactMetric(numeric::ExactMatrix::identity(2))};
 learning::ExactMetricChannelContext ctx(c,g,metrics);using K=learning::OperatorChannelKind;using T=learning::TowerOperatorChannelKind;using E=learning::ExactMetricChannelKind;
 for(unsigned raw=0;raw<256;++raw) {
  auto a=static_cast<learning::IntegerActivation>(raw);
  if(raw>=3) {
   rejects([&]{(void)learning::apply_integer_activation(a,-7);},"unknown activation executed");
   rejects([&]{learning::CompiledIntegerRelationalLayer l(c,g,{}, {},0,a);},"empty layer accepted bad activation");
   rejects([&]{learning::CompiledIntegerTowerLayer l(c,{}, {},0,a);},"empty tower accepted bad activation");
  } else { check(learning::apply_integer_activation(a,4)==(raw==2?1:4),"valid activation changed"); }
  if(raw>=4) {
   auto k=static_cast<K>(raw);auto t=static_cast<T>(raw);
   rejects([&]{(void)learning::apply_operator_channel(k,x);},"unknown machine operator became zero");
   rejects([&]{(void)learning::apply_operator_channel(k,ex);},"unknown exact operator became zero");
   rejects([&]{learning::CompiledIntegerRelationalLayer l(c,g,{k},{0});},"bad zero-gain operator accepted");
   rejects([&]{learning::CompiledIntegerTowerLayer l(c,{t},{0});},"bad zero-gain tower accepted");
   rejects([&]{(void)learning::reduce_operator_channels(c,g,{k});},"bad candidate reduced away");
   rejects([&]{(void)learning::apply_tower_operator_channel(t,tx);},"bad tower operator executed");
  }
  if(raw>=9) rejects([&]{learning::CompiledExactMetricLayer l(ctx,{{static_cast<E>(raw),0}},{0});},"bad metric channel skipped");
 }
 learning::IntegerRelationalNetwork n(c,g);
 rejects([&]{n.add_layer({other,g,{K::Identity},{1}});},"foreign-source layer inserted");check(n.size()==0,"failed layer insertion changed network");
 rejects([&]{n.add_layer({c,core::Grade{0},{K::Identity},{1}});},"foreign-grade layer inserted");
 n.add_layer({c,g,{K::Identity},{1}});check(n.forward(x).values()==x.values(),"identity network changed field");
 learning::IntegerTowerNetwork tower(c);rejects([&]{tower.add_layer({other,{T::Identity},{1}});},"foreign tower inserted");check(tower.size()==0,"failed tower insertion mutated");
 for(std::size_t width: {std::size_t{0},std::size_t{1},std::size_t{3},std::size_t{4}}) {
  numeric::RationalVector bad(width,0);
  rejects([&]{(void)ctx.apply({E::Identity,0},bad);},"identity erased carrier contract");
  rejects([&]{learning::CompiledExactMetricLayer l(ctx,{{E::Hodge,0}},{0});(void)l.forward(bad);},"zero layer erased carrier contract");
 }
 rejects([&]{learning::CompiledExactMetricLayer l(ctx,{{E::Identity,1}},{0});},"unused parameter silently ignored");
 for(auto lambda:{-1,0})rejects([&]{learning::CompiledExactMetricLayer l(ctx,{{E::GreenResolvent,lambda}},{0});},"invalid Green parameter skipped");
 numeric::Rational bad=1;mpz_set_ui(mpq_denref(bad.get_mpq_t()),0);
 rejects([&]{(void)learning::compile_exact_metric_layer(ctx,{{E::Identity,0}},{bad});},"compiler accepted invalid rational gain");

 rejects([&]{learning::CompiledExactMetricLayer l(ctx,{{E::Identity,0}},{bad});},"invalid rational gain admitted");
 rejects([&]{learning::CompiledExactMetricLayer l(ctx,{{E::Identity,0}},{0});(void)l.forward({bad,0});},"zero gain hid invalid value");
 auto basis=learning::reduce_exact_metric_channels(ctx,{{E::Identity,0},{E::HodgeDown,0},{E::HodgeUp,0},{E::Hodge,0}});
 auto poisoned=basis;poisoned.coordinates.clear();rejects([&]{(void)learning::reduce_exact_metric_gains(poisoned,{1,1,1,1});},"malformed certificate memory access");
 auto tower_basis=learning::reduce_tower_operator_channels(c,{T::Identity});tower_basis.coordinates.clear();
 rejects([&]{(void)learning::reduce_machine_tower_channel_gains(tower_basis,{1});},"malformed tower coordinates indexed");
 auto machine_basis=learning::reduce_operator_channels(c,g,{K::Identity});machine_basis.coordinates[0][0]=bad;
 rejects([&]{(void)learning::reduce_machine_channel_gains(machine_basis,{1});},"malformed machine-basis rational used");

 auto small=learning::ExactMetricAnalysisLimits{};small.max_rational_slots=1;
 rejects([&]{(void)learning::reduce_exact_metric_channels(ctx,{{E::Identity,0}},small);},"analysis exceeded budget");
 auto enormous=learning::ExactMetricAnalysisLimits{SIZE_MAX,SIZE_MAX,SIZE_MAX,SIZE_MAX};
 rejects([&]{learning::check_exact_metric_analysis_size(SIZE_MAX,2,enormous);},"analysis shape wrapped");
 learning::CompiledExactMetricLayer direct(ctx,{{E::Identity,0},{E::HodgeDown,0},{E::HodgeUp,0},{E::Hodge,0}},{2,3,4,5});
 auto optimized=learning::compile_exact_metric_layer(ctx,{{E::Identity,0},{E::HodgeDown,0},{E::HodgeUp,0},{E::Hodge,0}},{2,3,4,5});
 for(int a=-8;a<=8;++a)for(int b=-8;b<=8;++b){numeric::RationalVector value{numeric::rational(a,3),numeric::rational(b,5)};auto y=optimized.layer.forward(value);check(y==direct.forward(value),"operator rewrite changed full field");check(y.size()==2,"operator rewrite reduced coordinates");}
 check(numeric::rational(1,-2)==numeric::rational(-1,2),"negative denominator regression");
}
void local_integrity() {
 auto c=source();core::Grade g{1};local::LinearStalkBundle d(c);
 rejects([&]{d.set_dimension({core::Grade{0},0},SIZE_MAX);},"stalk sum wrapped");check(d.dimension({core::Grade{0},0})==1,"failed dimension update mutated");
 rejects([&]{local::LinearStalkBundle bad(c,SIZE_MAX);},"initial stalk total wrapped");
 d.set_dimension({core::Grade{0},0},2);local::LinearRelationalFiberSystem f(d);
 rejects([&]{local::GradeTensorField action(f,g);(void)action.apply({0,0});},"zero field hid missing transport");
 f.set_transport(g,0,numeric::ExactMatrix(2,1));local::GradeTensorField action(f,g);check(action.apply({0,0})==numeric::RationalVector(4,0),"valid zero vector lost width");
 local::LinearStalkBundle ones(c);local::LinearRelationalFiberSystem good(ones);local::LinearOccurrenceCover cover(good.stalks(),g);
 auto section=good.realize_relation(g,0,{3});auto y=cover.scatter_sections({section});check(y.size()==4,"occurrence support lost");
 auto wrong=section;wrong.relation.index=1;rejects([&]{(void)cover.scatter_sections({wrong});},"wrong occurrence owner accepted");
 wrong=section;wrong.boundary_values[0].lower={core::Grade{0},2};rejects([&]{(void)cover.scatter_sections({wrong});},"wrong lower participant accepted");
 wrong=section;wrong.upper_value={};rejects([&]{(void)cover.scatter_sections({wrong});},"wrong upper carrier accepted");
 rejects([&]{(void)cover.scatter_sections({section,section});},"duplicated section accepted");
 rejects([&]{local::LocalRelationTensor tensor({g,0},1,{{{core::Grade{0},0},0,0,numeric::ExactMatrix(SIZE_MAX,1)},{{core::Grade{0},1},1,SIZE_MAX,numeric::ExactMatrix(1,1)}});},"block offsets wrapped");
 local::LinearStalkBundle large(c);large.set_dimension({core::Grade{0},1},SIZE_MAX/2+1);
 rejects([&]{local::LinearOccurrenceCover x(large,g);},"repeated occurrence offsets wrapped");
 learning::BranchTypeGainLayer gains(good,g);rejects([&]{gains.set_gain(core::RelationTypeId{60000},1);},"nonexistent gain owner accepted");
 numeric::Rational bad=1;mpz_set_ui(mpq_denref(bad.get_mpq_t()),0);rejects([&]{gains.set_gain(core::RelationTypeId{0},bad);},"zero gain denominator reached GMP normalization");
}
void integer_edges(const std::filesystem::path& out) {
 std::ofstream csv;if(!out.empty()){csv.open(out/"multiplication.csv");csv<<"left,right,overflow,value\n";}
 std::mt19937_64 random(9282026);const auto lo=std::numeric_limits<std::int64_t>::min(),hi=std::numeric_limits<std::int64_t>::max();
 const numeric::Integer lower(std::to_string(lo)),upper(std::to_string(hi));
 auto test=[&](std::int64_t a,std::int64_t b){numeric::Integer expected=numeric::Integer(std::to_string(a))*numeric::Integer(std::to_string(b));bool overflow=expected<lower||expected>upper;
  if(overflow){rejects([&]{(void)numeric::checked_mul_i64_portable(a,b);},"portable multiply failed to reject");rejects([&]{(void)numeric::checked_mul_i64(a,b);},"native multiply failed to reject");}
  else {auto x=numeric::checked_mul_i64_portable(a,b);check(numeric::Integer(std::to_string(x))==expected,"portable multiply changed exact value");check(x==numeric::checked_mul_i64(a,b),"portable/native disagreement");}
  if(csv)csv<<a<<','<<b<<','<<overflow<<','<<(overflow?std::string{}:expected.get_str())<<'\n';};
 for(auto a:std::array<std::int64_t,9>{lo,lo+1,-4294967296LL,-1,0,1,4294967296LL,hi-1,hi})for(auto b:std::array<std::int64_t,9>{lo,lo+1,-4294967296LL,-1,0,1,4294967296LL,hi-1,hi})test(a,b);
 for(unsigned i=0;i<4096;++i){auto a=std::bit_cast<std::int64_t>(random()),b=std::bit_cast<std::int64_t>(random());if(i%3==0){a/=4294967296LL;b/=4294967296LL;}if(i%7==0)b=-1;test(a,b);}
}
}
static_assert(!std::is_constructible_v<learning::IntegerRelationalNetwork,core::RelationalComplex&&,core::Grade>);
static_assert(!std::is_constructible_v<local::LinearStalkBundle,core::RelationalComplex&&>);
static_assert(!std::is_constructible_v<local::LinearOccurrenceCover,local::LinearStalkBundle&&,core::Grade>);
static_assert(!std::is_constructible_v<local::GradeTensorField,local::LinearRelationalFiberSystem&&,core::Grade>);
static_assert(!std::is_constructible_v<local::LinearCellularSheaf,local::LinearStalkBundle&&>);
static_assert(!std::is_constructible_v<learning::ExactMetricChannelContext,const core::RelationalComplex&,core::Grade,std::vector<field::ExactMetric>&&>);
int main(int argc,char**argv){try{std::filesystem::path out;if(argc>1){out=argv[1];std::filesystem::create_directories(out);}tags_and_types();local_integrity();integer_edges(out);std::cout<<"native model integrity checks="<<checks<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
