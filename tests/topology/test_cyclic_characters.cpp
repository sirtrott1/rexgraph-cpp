#include "cyclic_character_fixtures.hpp"
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>

using namespace cyclic_character_test;
namespace {
void check(bool value, const char* why) { if(!value) throw std::runtime_error(why); }
template<class Exception, class F> void rejects(F call, const char* why) {
    bool caught = false;
    try { call(); } catch(const Exception&) { caught = true; }
    check(caught, why);
}
void unit_checks() {
    static_assert(!std::is_default_constructible_v<Census>);
    static_assert(!std::is_default_constructible_v<Census::Sector>);
    for(const std::uint64_t n : {1,2,3,4,5,6,8,12}) {
        Guard g;
        const auto chain = prepare(cycle(n), n, g);
        const auto c = Census::prepare(chain, {}, g);
        std::vector<std::uint64_t> orders;
        std::size_t degrees = 0;
        for(const auto& sector : c.sectors()) {
            orders.push_back(sector.order());
            const auto field = sector.realization().character().field();
            std::size_t phi = 0;
            for(std::uint64_t j = 1; j <= sector.order(); ++j) if(std::gcd(j,sector.order()) == 1) ++phi;
            check(sector.weight() == phi, "certified weight differs from Euler phi");
            check(std::get<numeric::CyclotomicIrreducibility>(field.certificate()).order == sector.order(), "order certificate lost");
            for(std::uint64_t j = 1; j < sector.order(); ++j)
                check(sector.realization().character().power(j) != field.scalar(1), "nonprimitive character admitted");
            check(sector.realization().equivariant_source().same_owner(chain), "sector source owner lost");
            check(sector.homology_dimensions().size() == chain.source().grade_count(), "grade truncated");
            degrees += sector.weight();
        }
        std::vector<std::uint64_t> expected;
        for(std::uint64_t d = 1; d <= n; ++d) if(n%d == 0) expected.push_back(d);
        check(orders == expected && degrees == n, "incomplete divisor census");
        check(c.homology_dimensions() == std::vector<std::size_t>({1,1}), "cycle aggregate differs");
        check(c.work_receipt().degree_sum == n && c.work_receipt().guard_steps > 0, "work receipt incomplete");
    }
    std::cout << "cyclic-character-T01 complete divisors, certified primitive fields and all-grade dimensions passed\n";
    Guard g;
    for(const std::uint32_t n : {3,4,5,6}) {
        const auto moore = Census::prepare(prepare(lens(n,1,false),n,g), {}, g);
        check(moore.homology_dimensions() == std::vector<std::size_t>({1,0,n-1}), "Moore top homology lost");
    }
    const auto full = Census::prepare(prepare(lens(7,2),7,g), {}, g);
    check(full.homology_dimensions() == std::vector<std::size_t>({2,0,0,2}), "disconnected lens grade3 lost");
    const auto nothing = Census::prepare(prepare(empty(),4,g), {}, g);
    check(nothing.homology_dimensions() == std::vector<std::size_t>({0,0,0,0}), "declared empty grades discarded");
    std::cout << "cyclic-character-T04 Moore, lens, direct sum and empty grades passed\n";
    auto decorated = prepare(lens(6,1,true,true),6,g);
    auto plain = prepare(lens(6),6,g);
    const auto dc = Census::prepare(decorated, {}, g);
    check(dc.source().voltages() == decorated.voltages(), "occurrence voltage changed");
    check(dc.source().source().grade(core::Grade{1}).incidence_count() == 6, "retained occurrences discarded");
    check(dc.work_receipt().source_occurrences == 14, "receipt omitted cancelling/zero/inactive source");
    const auto plain_census = Census::prepare(plain,{},g);
    check(dc.homology_dimensions() == plain_census.homology_dimensions(), "decorations changed numeric result");
    std::cout << "cyclic-character-T05 repeated, cancelled, inactive and present-zero occurrences passed\n";
    const auto nr = prepare(fractional(),6,g), ci = prepare(fractional(),6,g,Basis::CoordinatedIntegral);
    const auto nc = Census::prepare(nr,{},g), ic = Census::prepare(ci,{},g);
    check(nc.source().coefficient_basis() == Basis::NativeRational && ic.source().coefficient_basis() == Basis::CoordinatedIntegral,
          "coefficient basis erased");
    check(nr.formal_boundary(core::Grade{1}) != ci.formal_boundary(core::Grade{1}), "fractional fixture has no scale distinction");
    check(nc.homology_dimensions() == std::vector<std::size_t>({1,1,0,0,0}), "fractional/empty grade result differs");
    std::cout << "cyclic-character-T06 explicit rational/integral bases and source scales passed\n";
    auto bad = lens(6); bad.voltages[2][1] = 0;
    rejects<std::invalid_argument>([&]{(void)prepare(bad,6,g);}, "nonflat group-ring chain accepted");
    bad = lens(6,1,true,true); bad.voltages[1].pop_back();
    rejects<std::invalid_argument>([&]{(void)prepare(bad,6,g);}, "missing inactive voltage accepted");
    bad = lens(6); bad.voltages[1][0] = 6;
    rejects<std::out_of_range>([&]{(void)prepare(bad,6,g);}, "invalid residue accepted");
    std::cout << "cyclic-character-T07 malformed full-chain declarations refused\n";
    const auto pinned = [&]{ Guard local; return Census::prepare(prepare(cycle(6),6,local),{},local); }();
    const auto foreign = prepare(cycle(6),6,g);
    pinned.require_source(pinned.source());
    rejects<std::invalid_argument>([&]{pinned.require_source(foreign);}, "equal keys authorized a foreign owner");
    const auto copied = pinned;
    check(copied.same_owner(pinned), "copy lost census owner");
    check(!Census::prepare(pinned.source(),{},g).same_owner(pinned), "reconstruction reused census identity");
    check(pinned.sectors().back().realization().homology_dimensions(g) == pinned.sectors().back().homology_dimensions(),
          "destroyed wrappers invalidated retained sector");
    std::cout << "cyclic-character-T08 foreign owners refused and retained owners survive wrappers\n";
    const auto chain = prepare(cycle(12),12,g);
    numeric::AlgebraicFieldLimits fl;
    fl.max_degree = 1;
    rejects<std::length_error>([&]{Guard a;(void)Census::prepare(chain,fl,a);}, "degree limit ignored");
    fl = {}; fl.max_input_coefficients = 12;
    rejects<std::length_error>([&]{Guard a;(void)Census::prepare(chain,fl,a);}, "polynomial precharge ignored");
    fl = {}; fl.max_scalar_steps = 0;
    rejects<std::length_error>([&]{Guard a;(void)Census::prepare(chain,fl,a);}, "scalar work limit ignored");
    auto limits = numeric::ExactActionLimits{}; limits.dimension = 11;
    rejects<std::length_error>([&]{Guard a(limits);(void)Census::prepare(chain,{},a);}, "deck dimension limit ignored");
    limits = {}; limits.steps = 10;
    rejects<std::length_error>([&]{Guard a(limits);(void)Census::prepare(chain,{},a);}, "action work limit ignored");
    limits = {}; limits.integer_bits = 1;
    rejects<std::length_error>([&]{Guard a(limits);(void)Census::prepare(nr,{},a);}, "source coefficient bits ignored");
    Guard complete;
    const auto complete_census = Census::prepare(chain,{},complete);
    const auto receipt = complete_census.work_receipt();
    limits = {}; limits.entries = receipt.retained_entries_bound + receipt.scalar_scratch_entries_bound - 1;
    rejects<std::length_error>([&]{Guard a(limits);(void)Census::prepare(chain,{},a);}, "complete census storage precharge ignored");
    for(const auto point : {std::size_t{1}, complete.steps()/2, complete.steps()}) {
        std::size_t calls = 0;
        std::optional<Census> result;
        rejects<std::runtime_error>([&]{Guard a({},[&]{if(++calls == point)throw std::runtime_error("cancel");});
                                      result = Census::prepare(chain,{},a);}, "cancellation ignored");
        check(!result, "cancelled construction published a partial census");
    }
    const auto recovered = Census::prepare(chain,{},g);
    check(recovered.homology_dimensions() == std::vector<std::size_t>({1,1}), "refusal mutated source");
    std::cout << "cyclic-character-T09 resource/refusal and early/middle/final cancellation passed\n";
    // There is deliberately no caller-supplied mutable sector list or public
    // census/sector constructor. Wrong/nonprimitive imported lists cannot enter.
    check(!std::is_default_constructible_v<Census::Sector>, "unsafe imported sector route");
    std::cout << "cyclic-character-T10 complete sectors constructed internally; no imported-list authority\n";
}

template<class T> void numbers(const std::vector<T>& values) {
    std::cout << '[';
    for(std::size_t i=0;i<values.size();++i) { if(i)std::cout<<',';std::cout<<values[i]; }
    std::cout << ']';
}
void emit(const char* name, Declaration input, std::uint64_t n, Basis basis, bool& first) {
    Guard g;
    const auto chain = prepare(std::move(input),n,g,basis);
    const auto census = Census::prepare(chain,{},g);
    if(!first)std::cout<<',';
    first=false;
    std::cout << "{\"name\":\"" << name << "\",\"n\":" << n << ",\"basis\":\""
              << (basis==Basis::NativeRational?"NativeRational":"CoordinatedIntegral") << "\",\"cells\":";
    std::vector<std::size_t> cells;
    for(std::size_t k=0;k<chain.source().grade_count();++k)cells.push_back(chain.source().grade(topology::equivariant_detail::grade(k)).cell_count());
    numbers(cells);std::cout << ",\"occurrences\":[";
    bool first_occ=true;
    for(std::size_t k=1;k<cells.size();++k) {
        const auto grade=topology::equivariant_detail::grade(k);
        const auto& arena=chain.source().grade(grade);
        for(std::size_t slot=0;slot<arena.incidence_count();++slot) {
            if(!first_occ)std::cout<<',';
            first_occ=false;
            const auto q = basis==Basis::NativeRational?chain.source().incidence_coefficient(grade,slot):Q{arena.lattice_coefficient[slot]};
            std::cout << '[' << k << ',' << arena.lower_index[slot] << ',' << arena.upper_index[slot] << ','
                      << chain.voltages()[k][slot] << ',' << (arena.aspects.get(chain.source().signature().existence_aspect(),slot)?1:0)
                      << ",\"" << q << "\"]";
        }
    }
    std::cout << "],\"total\":";numbers(census.homology_dimensions());std::cout << ",\"sectors\":[";
    bool first_sector=true;
    for(const auto& sector:census.sectors()) {
        if(!first_sector)std::cout<<',';
        first_sector=false;
        std::cout << "{\"order\":" << sector.order() << ",\"weight\":" << sector.weight() << ",\"betti\":";
        numbers(sector.homology_dimensions());std::cout << '}';
    }
    std::cout << "]}";
}
void reference_inputs() {
    bool first=true;
    std::cout << '[';
    for(const std::uint64_t n:{1,2,3,4,5,6,8,12})emit("cycle",cycle(n),n,Basis::NativeRational,first);
    for(const std::uint64_t n:{3,6})emit("torus-self",polygon({1,n==6?1u:0u},{{0,1},{1,1},{0,-1},{1,-1}},n),n,Basis::NativeRational,first);
    emit("klein-orientation",polygon({0,1},{{0,1},{1,1},{0,1},{1,-1}},2),2,Basis::NativeRational,first);
    emit("projective-orientation",polygon({1},{{0,1},{0,1}},2),2,Basis::NativeRational,first);
    for(const std::uint32_t n:{3,4,5,6})emit("moore",lens(n,1,false),n,Basis::NativeRational,first);
    emit("lens",lens(7),7,Basis::NativeRational,first);
    emit("lens-direct-sum",lens(7,2),7,Basis::NativeRational,first);
    emit("retained-decorations",lens(6,1,true,true),6,Basis::NativeRational,first);
    emit("empty",empty(),4,Basis::NativeRational,first);
    emit("fractional",fractional(),6,Basis::NativeRational,first);
    emit("fractional",fractional(),6,Basis::CoordinatedIntegral,first);
    std::cout << "]\n";
}
} // namespace
int main(int argc, char** argv) {
    try {
        if(argc==2 && std::string(argv[1])=="--reference")reference_inputs();
        else unit_checks();
    } catch(const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n';return 1; }
}
