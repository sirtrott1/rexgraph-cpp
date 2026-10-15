#include "rfc/local/radix_carry.hpp"
#include "rfc/local/tensor_field.hpp"
#include <iostream>
#include <numeric>
using namespace rfc;
using Q=numeric::Rational;
using V=numeric::RationalVector;
using M=numeric::ExactMatrix;
using Guard=numeric::ExactActionGuard;
using Carry=local::PreparedRadixCarry;
namespace {
std::size_t checks{};
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void rejects(F f,const char* reason){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,reason);}
std::vector<core::IdentityKey> positions(std::size_t n){std::vector<core::IdentityKey> out(n);std::iota(out.begin(),out.end(),100);return out;}
std::vector<core::RelationKey> relations(std::size_t n){std::vector<core::RelationKey> out;for(std::size_t i=1;i<n;++i)out.push_back(core::RelationKey{1000+i});return out;}
void certificate(const Carry& owner,const Carry::Transition& receipt,Guard& guard) {
    auto shifted=owner.boundary().apply(receipt.carries());
    for(std::size_t i=0;i<shifted.size();++i)shifted[i]+=receipt.before().coordinates()[i];
    check(shifted==receipt.after().coordinates(),"retained carry reconstruction differs");
    check(receipt.before().same_owner(receipt.after()),"carry receipt changed field owner");
    check(owner.value(receipt.before(),guard)==receipt.value() && owner.value(receipt.after(),guard)==receipt.value(),"carry value receipt differs");
}
void rational_invariant() {
    Guard guard;auto ids=positions(6);auto keys=relations(6);
    const auto carry=Carry::rational(Q{3,2},ids,keys,guard);
    check(carry.coordinates_per_position()==1 && carry.position_count()==6,"rational source shape differs");
    check(carry.source().grade(core::Grade{1}).incidence_count()==10,"carry occurrence flags lost");
    check(carry.boundary()==local::GradeTensorField(carry.owner().fibers(),core::Grade{1}).matrix(),"carry boundary differs from native occurrence maps");
    check(carry.boundary().rank()==5 && carry.value_map().multiply(carry.boundary()).nonzeros()==0,"complete rational carry annihilator differs");
    const auto input=carry.field({2,-3,7,Q{1,2},-1,9},guard);
    const auto receipt=carry.apply_carries(input,{Q{1,3},-7,3,Q{-5,2},10},guard);certificate(carry,receipt,guard);
    Q expected;for(std::size_t i=input.coordinates().size();i-->0;)expected=expected*Q{3,2}+input.coordinates()[i];
    check(receipt.value()==V{expected},"independent radix evaluation differs");
    check(input.coordinates()==V({2,-3,7,Q{1,2},-1,9}),"carry application mutated input field");
    const auto copied=carry;check(copied.same_owner(carry) && copied.owner().same_owner(carry.owner()),"copy lost native source owner");
    const auto same_values=Carry::rational(Q{3,2},ids,keys,guard);
    rejects([&]{(void)same_values.value(input,guard);},"equal values identified different carrier owners");
    ids[0]=9999;keys[0]=core::RelationKey{9999};
    check(carry.source().grade(core::Grade{0}).cell_count()==6 && carry.source().grade(core::Grade{1}).cell_count()==5,"external keys altered retained carrier");
    rejects([&]{(void)carry.normalize_integer(input,guard);},"fractional radix accepted for integer normalization");
    const auto zero=Carry::rational(Q{0},positions(3),relations(3),guard);
    const auto moved=zero.apply_carries(zero.field({1,2,3},guard),{7,-9},guard);certificate(zero,moved,guard);
    check(moved.value()==V{1} && zero.source().grade(core::Grade{1}).incidence_count()==4,"zero coefficient incidence was discarded");
}
void integer_normalization() {
    Guard guard;const auto positive=Carry::rational(Q{10},positions(5),relations(5),guard);
    const auto receipt=positive.normalize_integer(positive.field({1234,-17,2,0,0},guard),guard);
    certificate(positive,receipt,guard);check(receipt.after().coordinates()==V({4,6,2,1,0}),"decimal digits differ");
    const auto binary=Carry::rational(Q{2},positions(10),relations(10),guard);
    const auto negative=Carry::rational(Q{-2},positions(12),relations(12),guard);
    for(int integer=-128;integer<=128;++integer) {
        V field(12);field[0]=integer;
        const auto result=negative.normalize_integer(negative.field(field,guard),guard);
        certificate(negative,result,guard);check(result.value()==V{Q{integer}},"negative-base value differs");
        for(const auto& digit:result.after().coordinates())check(digit==0 || digit==1,"negative-base digit is not canonical");
        if(integer>=0) {
            field.resize(10);const auto ordinary=binary.normalize_integer(binary.field(field,guard),guard);
            certificate(binary,ordinary,guard);check(ordinary.value()==V{Q{integer}},"positive-base value differs");
        }
    }
    const auto small=Carry::rational(Q{2},positions(2),relations(2),guard);
    const auto overflow=small.field({4,0},guard);
    rejects([&]{(void)small.normalize_integer(overflow,guard);},"finite carrier overflow invented new positions");
    check(overflow.coordinates()==V({4,0}) && small.position_count()==2,"overflow changed retained input");
    rejects([&]{(void)small.normalize_integer(small.field({-1,0},guard),guard);},"negative integer has unsigned positive-base finite digits");
    rejects([&]{(void)small.normalize_integer(small.field({Q{1,2},0},guard),guard);},"fractional digit normalized as integer");
    const auto unit=Carry::rational(Q{1},positions(2),relations(2),guard);
    rejects([&]{(void)unit.normalize_integer(unit.field({1,0},guard),guard);},"base-one integer normalization accepted");
    const auto single=Carry::rational(Q{10},positions(1),relations(1),guard);
    const auto unchanged=single.normalize_integer(single.field({7},guard),guard);certificate(single,unchanged,guard);
    check(single.source().grade_count()==2 && unchanged.carries().empty(),"explicit empty carry grade lost");
}
void gaussian_normalization() {
    Guard guard;const auto carry=Carry::gaussian_minus_one_plus_i(positions(6),relations(6),guard);
    check(carry.radix()==V({-1,1}) && carry.coordinates_per_position()==2,"Gaussian scalar basis differs");
    check(carry.boundary()==local::GradeTensorField(carry.owner().fibers(),core::Grade{1}).matrix(),"Gaussian native occurrence action differs");
    check(carry.boundary().rank()==10 && carry.value_map().multiply(carry.boundary()).nonzeros()==0,"complete Gaussian value annihilator differs");
    V initial(12);initial[0]=5;initial[1]=-3;
    const auto receipt=carry.normalize_integer(carry.field(initial,guard),guard);certificate(carry,receipt,guard);
    check(receipt.after().coordinates()==V({0,0,1,0,1,0,1,0,0,0,1,0}),"paper Gaussian 101110 digit certificate differs");
    check(receipt.value()==V({5,-3}),"Gaussian represented value differs");
    const auto large=Carry::gaussian_minus_one_plus_i(positions(14),relations(14),guard);
    for(int real=-6;real<=6;++real)for(int imaginary=-6;imaginary<=6;++imaginary) {
        V input(28);input[0]=real;input[1]=imaginary;
        const auto result=large.normalize_integer(large.field(input,guard),guard);certificate(large,result,guard);
        Q a,b;
        for(std::size_t i=14;i-->0;) {
            const Q next_a=-a-b+result.after().coordinates()[2*i];
            b=a-b+result.after().coordinates()[2*i+1];a=next_a;
            check((result.after().coordinates()[2*i]==0 || result.after().coordinates()[2*i]==1) &&
                result.after().coordinates()[2*i+1]==0,"Gaussian digit is not zero or one");
        }
        check(a==real && b==imaginary,"independent Gaussian Horner reconstruction differs");
    }
    const auto small=Carry::gaussian_minus_one_plus_i(positions(5),relations(5),guard);initial.resize(10);
    rejects([&]{(void)small.normalize_integer(small.field(initial,guard),guard);},"Gaussian normalization synthesized undeclared sixth position");
    V rational_carries(10,Q{1,3});const auto moved=carry.apply_carries(receipt.after(),rational_carries,guard);certificate(carry,moved,guard);
    rejects([&]{(void)carry.normalize_integer(moved.after(),guard);},"nonintegral Gaussian digit normalized as Gaussian integer");
}
void refusals() {
    Guard guard;
    rejects([&]{(void)Carry::rational(Q{2},{},{},guard);},"empty positional carrier accepted");
    rejects([&]{(void)Carry::rational(Q{2},{1,1},{core::RelationKey{9}},guard);},"duplicate position identities accepted");
    rejects([&]{(void)Carry::rational(Q{2},{1,2,3},{core::RelationKey{9},core::RelationKey{9}},guard);},"duplicate carry identities accepted");
    rejects([&]{(void)Carry::rational(Q{2},{1,2},{},guard);},"missing adjacent carry accepted");
    rejects([&]{Guard canceled({},[]{throw std::runtime_error("cancel");});(void)Carry::rational(Q{2},positions(5),relations(5),canceled);},"cancellation ignored");
    rejects([&]{auto limits=numeric::ExactActionLimits{};limits.entries=8;Guard small(limits);(void)Carry::rational(Q{2},positions(5),relations(5),small);},"carrier admission ignored");
    rejects([&]{auto limits=numeric::ExactActionLimits{};limits.integer_bits=4;Guard small(limits);(void)Carry::rational(Q{10},positions(5),relations(5),small);},"radix power bit admission ignored");
    const auto carry=Carry::rational(Q{2},positions(2),relations(2),guard);
    rejects([&]{(void)carry.field({1},guard);},"incomplete digit coordinates accepted");
    const auto field=carry.field({1,0},guard);
    rejects([&]{(void)carry.apply_carries(field,{},guard);},"incomplete carry coordinates accepted");
}
}
int main(){try{rational_invariant();integer_normalization();gaussian_normalization();refusals();
    std::cout<<checks<<" radix carry checks passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
