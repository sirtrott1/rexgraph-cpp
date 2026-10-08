#include "rfc/core/builder.hpp"
#include "rfc/field/relational_channels.hpp"
#include <future>
#include <iostream>

using namespace rfc;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
namespace {
std::size_t checks{};
void check(bool condition, const char* message) {
    ++checks; if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F action, const char* message) {
    bool refused = false; try { action(); } catch (const std::exception&) { refused = true; }
    check(refused, message);
}
Q absq(const Q& q) { return q < 0 ? Q{-q} : q; }
core::RelationalComplex fixture(std::size_t seed) {
    core::RelationSignature signature;
    const auto type = signature.add_type("channel", core::BoundaryPolicy::DeclaredExact, 0, 8);
    core::RelationalComplexBuilder builder(signature);
    builder.declare_grade(core::Grade{2});
    for (std::size_t j = 0; j < 5; ++j) {
        std::vector<core::BoundaryOccurrenceInput> flags;
        for (std::size_t i = 0; i < 4; ++i) {
            const auto n = static_cast<unsigned long>((seed + 3*i + 5*j) % 7);
            flags.emplace_back(i, (seed + i + j) % 6 != 0, (seed + i + 2*j) % 3 == 0,
                Q{n, static_cast<unsigned long>(1 + (seed + i + j) % 4)});
        }
        builder.add_grade_one(core::RelationKey{100+j}, type, flags);
    }
    return builder.freeze();
}
std::array<M,4> oracle(const core::RelationalComplex& source, const V& w, bool count) {
    const auto& arena = source.grade(core::Grade{1});
    const auto n = w.size(), rows = source.grade(core::Grade{0}).cell_count();
    M b(rows,n), a(rows,n), ac(rows,n);
    for (std::size_t p = 0; p < arena.incidence_count(); ++p) {
        if (!arena.aspects.get(source.signature().existence_aspect(),p)) continue;
        const auto q = source.incidence_coefficient(core::Grade{1},p);
        const auto i = arena.lower_index[p], j = arena.upper_index[p];
        b.set(i,j,q*w[j]); a.set(i,j,absq(q)*w[j]); ac.set(i,j,count && q != 0 ? Q{1} : absq(q));
    }
    std::array<M,4> out{b.transpose().multiply(b), a.transpose().multiply(a), M(n,n), M(n,n)};
    const auto c = ac.transpose().multiply(ac);
    for (std::size_t i = 0; i < n; ++i) {
        Q fmass{0}, cmass{0};
        for (std::size_t j = 0; j < n; ++j) {
            const Q d = out[0].get(i,j)-out[1].get(i,j);
            out[2].set(i,j,d); fmass += absq(d); cmass += c.get(i,j);
            out[3].set(i,j,-c.get(i,j));
        }
        out[2].set(i,i,out[2].get(i,i)+fmass);
        out[3].set(i,i,out[3].get(i,i)+cmass);
    }
    return out;
}
void small_cases() {
    for (std::size_t seed = 0; seed < 32; ++seed) {
        const auto source = fixture(seed);
        V w(5), x(5), y(5);
        for (std::size_t i = 0; i < 5; ++i) {
            w[i] = Q{static_cast<long>((seed+i)%5)-2, 3};
            x[i] = Q{static_cast<long>((seed+2*i)%7)-3, 5};
            y[i] = Q{static_cast<long>((seed+3*i)%9)-4, 7};
        }
        for (const bool count : {false,true}) {
            field::PreparedRelationalChannels action(source, w, count ?
                field::CoparticipationReading::Count : field::CoparticipationReading::Share);
            const auto expected = oracle(source,w,count);
            const auto result = action.apply_all(x), other = action.apply_all(y);
            const auto normalized = action.apply_all(x,true);
            for (std::size_t k = 0; k < 4; ++k) {
                const auto exact = expected[k].apply(x);
                check(result.fields[k] == exact, "factored action differs from exact assembled oracle");
                check(action.apply(static_cast<field::RelationalChannel>(k),x) == exact, "selected channel differs");
                check(action.apply(static_cast<field::RelationalChannel>(k),x,true) == normalized.fields[k], "selected normalization differs");
                Q trace{0}, energy{0}, left{0}, right{0};
                for (std::size_t i = 0; i < x.size(); ++i) {
                    trace += expected[k].get(i,i); energy += x[i]*exact[i];
                    left += x[i]*other.fields[k][i]; right += y[i]*exact[i];
                    check(action.diagonals()[k][i] == expected[k].get(i,i), "channel diagonal differs");
                    check(normalized.fields[k][i] ==
                        (action.traces()[k] == 0 ? exact[i] : Q{exact[i]/action.traces()[k]}), "trace normalization differs");
                }
                check(action.traces()[k] == trace, "channel trace differs");
                check(result.energies[k] == energy && energy >= 0, "exact PSD energy differs");
                check(left == right, "Euclidean symmetry differs");
            }
            Q pair_energy{0};
            for (std::size_t i = 0; i < x.size(); ++i) for (std::size_t j = i+1; j < x.size(); ++j) {
                const Q d = expected[0].get(i,j)-expected[1].get(i,j);
                const Q sum = x[i] + (d < 0 ? -x[j] : x[j]);
                pair_energy += absq(d)*sum*sum;
            }
            check(pair_energy == result.energies[2], "signed frustration pair energy differs");
            check(action.apply(field::RelationalChannel::Frustration,x) == result.fields[2], "selected action differs");
            check(action.source().grade(core::Grade{1}).incidence_count() == 20, "original inactive or zero flags lost");
            check(action.source().grade_count() == 3, "empty upper grade lost");
        }
    }
}
void edge_cases() {
    const auto source = fixture(2);
    field::PreparedRelationalChannels zero(source,V(5));
    const auto result = zero.apply_all(V{1,2,3,4,5},true);
    for (std::size_t k = 0; k < 3; ++k) {
        check(zero.traces()[k] == 0 && result.energies[k] == 0, "zero weighted channel lost");
        check(result.fields[k] == V(5), "zero trace channel action is nonzero");
    }
    check(zero.traces()[3] > 0, "C incorrectly inherits zero relation weights");
    rejects([&]{ (void)zero.apply_all(V{1}); }, "foreign dimension accepted");
    rejects([&]{ (void)zero.apply(static_cast<field::RelationalChannel>(8),V(5)); }, "invalid channel accepted");
    rejects([&]{ field::PreparedRelationalChannels bad(source,V(4)); }, "partial weights accepted");
    rejects([&]{ field::PreparedRelationalChannels bad(source,V(5),static_cast<field::CoparticipationReading>(8)); }, "invalid C reading accepted");
    rejects([&]{ (void)zero.apply_all(V(5),false,[]{throw std::runtime_error("cancelled");}); }, "cancellation ignored");
    core::RelationSignature signature;
    const auto type = signature.add_type("repeat",core::BoundaryPolicy::DeclaredExact,0,8);
    core::RelationalComplexBuilder repeated(signature);
    repeated.add_grade_one(type,{{0,true,true,Q{1}},{0,true,false,Q{1}}});
    rejects([&]{ field::PreparedRelationalChannels bad(repeated.freeze(),V{1}); }, "repeated participant accepted");
    core::RelationalComplexBuilder empty(signature); empty.declare_grade(core::Grade{1});
    field::PreparedRelationalChannels empty_action(empty.freeze(),{});
    check(empty_action.apply_all({}).fields[0].empty(), "empty carrier failed");
    numeric::ExactActionLimits tiny; tiny.entries = 1;
    rejects([&]{ field::PreparedRelationalChannels bad(source,V(5),field::CoparticipationReading::Share,tiny); }, "entry admission ignored");
    tiny = {}; tiny.integer_bits = 1;
    rejects([&]{ field::PreparedRelationalChannels bad(source,V(5,Q{8}),field::CoparticipationReading::Share,tiny); }, "bit admission ignored");
}
void hub_case() {
    constexpr std::size_t n = 1024;
    core::RelationSignature signature;
    const auto type = signature.add_type("hub",core::BoundaryPolicy::DeclaredExact,2,2);
    core::RelationalComplexBuilder builder(signature);
    for (std::size_t i = 0; i < n; ++i)
        builder.add_grade_one(type,{{0,true,i%2==0,Q{1}},{i+1,true,i%2!=0,Q{1}}});
    numeric::ExactActionLimits limits; limits.entries = 30000;
    field::PreparedRelationalChannels action(builder.freeze(),V(n,Q{1}),field::CoparticipationReading::Count,limits);
    const auto result = action.apply_all(V(n,Q{1}));
    check(action.retained_entries() < limits.entries, "hub retained storage exceeds linear budget");
    check(action.preparation_steps() < 100*n && result.steps < 100*n, "hub enumerates relation pairs");
    for (std::size_t i = 0; i < n; ++i) {
        check(result.fields[0][i] == 1, "hub T oracle differs");
        check(result.fields[1][i] == n+1, "hub raw G oracle differs");
        check(result.fields[2][i] == 0 && result.fields[3][i] == 0, "hub difference energy differs");
    }
    auto first = std::async(std::launch::async,[&]{ return action.apply_all(V(n,Q{1})); });
    auto second = std::async(std::launch::async,[&]{ return action.apply_all(V(n,Q{2})); });
    check(first.get().fields == result.fields, "immutable concurrent action differs");
    check(second.get().energies[0] == 4*result.energies[0], "concurrent workspaces interfere");
}
}
int main() {
    small_cases(); edge_cases(); hub_case();
    std::cout << "relational channel checks " << checks << '\n';
}
