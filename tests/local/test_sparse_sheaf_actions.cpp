#include "rfc/local/sparse_sheaf_actions.hpp"
#include "rfc/core/builder.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
using namespace rfc;
using Q = numeric::Rational;
using M = numeric::ExactMatrix;
using V = numeric::RationalVector;
using S = numeric::SparseRationalVector;
using F = local::SparseSheafCochain;
using A = local::SparseSheafActions;
using O = local::OwnedLinearCellularSheaf;
std::size_t checks{}, oracle_fields{}, exact_coordinates{};

void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class Ex, class Fn> void rejects(Fn fn, const char* message) {
    bool caught = false;
    try { fn(); } catch (const Ex&) { caught = true; }
    check(caught, message);
}
core::Grade g(std::size_t k) { return {static_cast<std::uint16_t>(k)}; }
M rotation() { return M::from_rows({{Q{3,5},Q{-4,5}},{Q{4,5},Q{3,5}}}); }
core::RelationalComplex triangle(bool extra = false, std::size_t copies = 1u) {
    core::RelationSignature sig;
    const auto type = sig.add_type("sheaf-flag", core::BoundaryPolicy::DeclaredExact, 0, 16);
    core::RelationalComplexBuilder b(sig);
    for (std::size_t i = 0; i < copies; ++i) {
        const auto a = 10u + 3u*i, c = a+1u, d = a+2u;
        if (extra) b.add_grade_one(type, {{a,true,true,1},{c,true,false,1},
            {a,true,false,Q{1,2}},{a,true,true,Q{1,2}},
            {d,true,false,0},{c,false,false,7}});
        else b.add_grade_one(type, {{a,true,true,1},{c,true,false,1}});
        b.add_grade_one(type, {{c,true,true,1},{d,true,false,1}});
        b.add_grade_one(type, {{d,true,true,1},{a,true,false,1}});
    }
    for (std::size_t i = 0; i < copies; ++i) {
        const auto a = static_cast<std::uint32_t>(3*i);
        b.add_higher(g(2), type, {{a,true,false,1},{a+1u,true,false,1},{a+2u,true,false,1}});
    }
    return b.freeze();
}
O fixture(int profile, std::size_t n = 2u, std::size_t copies = 1u) {
    const auto source = triangle(profile == 3, copies);
    local::LinearStalkBundle bundle(source, n);
    if (profile == 3) {
        for (std::size_t k=0;k<source.grade_count();++k)
            for (std::uint32_t c=0;c<source.grade(g(k)).cell_count();++c)
                bundle.set_dimension({g(k),c}, 2u + ((c+k)%3u));
    }
    local::LinearCellularSheaf sheaf(bundle);
    if (profile == 1) sheaf.set_restriction(g(1), 1u, rotation());
    if (profile == 2 || profile == 3 || profile == 4) {
        for (std::size_t k=1;k<source.grade_count();++k) {
            const auto& ar=source.grade(g(k));
            for (std::size_t slot=0;slot<ar.incidence_count();++slot) {
                const auto rows=bundle.dimension({g(k),ar.upper_index[slot]});
                const auto cols=bundle.dimension({g(k-1),ar.lower_index[slot]});
                M rho(rows,cols);
                if (profile == 2) {
                    const Q scale{static_cast<unsigned long>(ar.upper_index[slot]+k+2),
                                  static_cast<unsigned long>(ar.lower_index[slot]+k+1)};
                    for (std::size_t j=0;j<n;++j) rho.set(j,j,scale);
                } else if (profile == 4) {
                    rho.set(0,0,1);rho.set(0,n-1,2);rho.set(n-1,0,-1);rho.set(n-1,n-1,3);
                } else {
                    for (std::size_t i=0;i<rows;++i) for (std::size_t j=0;j<cols;++j) {
                        const auto numerator=static_cast<long>((i+1)*(j+2)+slot%3);
                        if ((i+j+slot)%4) rho.set(i,j,Q{((i+slot)%2?-1:1)*numerator,
                                                           static_cast<unsigned long>(2+i+j)});
                    }
                }
                sheaf.set_restriction(g(k),slot,std::move(rho));
            }
        }
    }
    return O(sheaf);
}
V flattened(const F& field) {
    // Independent bounded reference only. Never called in the scaling cases.
    const auto& bundle=field.owner().sheaf().stalks();
    const auto offsets=bundle.offsets(field.grade());
    V result(offsets.back(),Q{0});
    for (const auto& [cell,value]:field.values()) for (const auto& entry:value.entries())
        result[offsets[cell]+entry.index]=entry.value;
    return result;
}
void expect(const F& actual, const V& expected, const char* message) {
    const auto found=flattened(actual);
    check(found.size()==expected.size(),"reference dimension");
    for (std::size_t i=0;i<found.size();++i) {check(found[i]==expected[i],message);++exact_coordinates;}
    ++oracle_fields;
}
bool same(const F& a,const F& b) {
    if (!a.owner().same_owner(b.owner()) || a.grade()!=b.grade() || a.values().size()!=b.values().size())return false;
    auto l=a.values().begin(),r=b.values().begin();
    for(;l!=a.values().end();++l,++r)
        if(l->first!=r->first || l->second.size()!=r->second.size() || l->second.entries()!=r->second.entries())return false;
    return true;
}
void compact_oracles() {
    for(int profile=0;profile<4;++profile) {
        const A action(fixture(profile));
        const auto& sheaf=action.owner().sheaf();
        for(std::size_t k=0;k+1<sheaf.complex().grade_count();++k) {
            const auto reference=sheaf.coboundary(g(k));
            for(std::uint32_t cell=0;cell<sheaf.complex().grade(g(k)).cell_count();++cell) {
                const auto n=sheaf.stalks().dimension({g(k),cell});
                for(std::size_t c=0;c<n;++c) {
                    const auto input=action.bind(g(k),{{cell,S::basis(n,c,Q{2,3})}});
                    expect(action.coboundary(input).field,reference.apply(flattened(input)),"basis upward mismatch");
                    if(k==0) {
                        const auto defect=action.two_step_defect(input);
                        expect(defect.field,sheaf.two_step_defect(g(0)).apply(flattened(input)),"basis two-step mismatch");
                        if(profile==0||profile==2)check(defect.field.zero(),"flat fixture cochain closure");
                    }
                }
            }
            for(int seed=0;seed<12;++seed) {
                local::SparseSheafValues values;
                for(std::uint32_t cell=0;cell<sheaf.complex().grade(g(k)).cell_count();++cell) {
                    const auto n=sheaf.stalks().dimension({g(k),cell});
                    std::vector<numeric::SparseRationalEntry> entries;
                    for(std::size_t c=0;c<n;++c) {
                        Q q{static_cast<long>((seed+cell*2+c*3)%7)-3,static_cast<unsigned long>(c+1)};
                        if(q!=0) entries.push_back({c,q});
                    }
                    values.emplace(cell,S(n,std::move(entries)));
                }
                const auto input=action.bind(g(k),values);
                const auto output=action.coboundary(input);
                expect(output.field,reference.apply(flattened(input)),"sample upward mismatch");
                if(k==0)expect(action.two_step_defect(input).field,
                               sheaf.two_step_defect(g(0)).apply(flattened(input)),"sample two-step mismatch");
            }
        }
    }
    const A twisted(fixture(1));
    const auto bad=twisted.two_step_defect(twisted.bind(g(0),{{1,S::basis(2,0)}}));
    expect(bad.field,V{Q{-2,5},Q{4,5}},"rotation defect R-I");
    check(!bad.field.zero(),"curved upward maps were silently repaired");
}
void occurrences_and_direction() {
    core::RelationSignature sig;
    const auto type=sig.add_type("signed-occurrences",core::BoundaryPolicy::DeclaredExact,0,16);
    core::RelationalComplexBuilder b(sig);
    b.add_grade_one(type,{{10,true,true,1},{10,true,false,1}});
    const auto source=b.freeze();
    local::LinearStalkBundle bundle(source,2);
    local::LinearCellularSheaf sheaf(bundle);
    sheaf.set_restriction(g(1),1,rotation());
    const A action{O(sheaf)};
    const auto input=action.bind(g(0),{{0,S::basis(2,0)}});
    expect(action.coboundary(input).field,V{Q{-2,5},Q{4,5}},"cancelled scalar occurrences lost directional distinction");
    check(source.incidence_coefficient(g(1),0)+source.incidence_coefficient(g(1),1)==0,"scalar cancellation fixture");
    check(action.owner().sheaf().complex().grade(g(1)).incidence_count()==2,"occurrence count lost");
    check(action.owner().sheaf().complex().grade(g(1)).occurrence_identities==source.grade(g(1)).occurrence_identities,"occurrence identities lost");

    core::RelationalComplexBuilder w(sig);
    w.add_grade_one(type,{{10,true,true,Q{2,3}},{20,true,false,Q{1,2}},
                          {10,true,false,Q{1,5}},{10,true,true,Q{1,5}},
                          {10,false,false,7},{10,true,false,0}});
    const auto weighted=w.freeze();
    local::LinearStalkBundle wb(weighted,2);
    local::LinearCellularSheaf ws(wb);
    const M nonsymmetric=M::from_rows({{0,2},{3,0}});
    ws.set_restriction(g(1),0,nonsymmetric);
    ws.set_restriction(g(1),1,M(2,2));
    ws.set_restriction(g(1),4,nonsymmetric);
    ws.set_restriction(g(1),5,nonsymmetric);
    const A wa{O(ws)};
    const auto result=wa.coboundary(wa.bind(g(0),{{0,S(2,{{0,3},{1,5}})},{1,S(2,{{0,100},{1,200}})}}));
    expect(result.field,V{Q{-20,3},Q{-6}},"upward rho sign/weight/transpose semantics");
    check(result.work.inactive_incidences==1,"inactive flag executed");
    check(result.work.zero_coeff_incidences==1,"zero coefficient flag executed");
    check(wa.owner().sheaf().restriction_override(g(1),1).has_value(),"explicit zero became implicit identity");
    const auto zero=wa.bind(g(0),{{0,S(2)},{1,S(2)}});
    check(zero.zero(),"zero values retained numerically");
    check(wa.coboundary(zero).work.incidences==0,"zero input scanned cofaces");
    check(wa.owner().sheaf().complex().grade(g(1)).incidence_count()==6,"zero field erased source flags");
}

struct Record {std::size_t dimension; int mode; local::SparseSheafPreparationWork prep; local::SparseSheafActionWork up, defect;};
std::vector<Record> records;
void scaling() {
    for(int mode:{0,4}) {
        std::optional<Record> first;
        for(std::size_t n:{std::size_t{2},std::size_t{257},std::size_t{1'000'003}}) {
            auto limits=A::default_limits();limits.entries=512;limits.steps=10'000;limits.integer_bits=256;
            const A action(fixture(mode,n),limits);
            const auto input=action.bind(g(0),{{0,S(n,{{0,Q{3,7}},{n-1,Q{-5,11}}})}});
            const auto up=action.coboundary(input),defect=action.two_step_defect(input);
            check(up.field.nonzeros()==4,"sparse result support");
            check(defect.field.zero(),"large field chain did not close");
            check(up.work.incidences==2,"upward action scanned unrelated incidences");
            check(up.field.values().at(0).size()==n,"stalk extent compressed");
            check(up.field.values().at(0).entries().back().index==n-1,"original coordinate lost");
            Record current{n,mode,action.preparation(),up.work,defect.work};
            if(first) {
                check(current.prep==first->prep,"preparation depends on unused stalk coordinates");
                check(current.up==first->up,"upward work depends on unused stalk coordinates");
                check(current.defect==first->defect,"defect work depends on unused stalk coordinates");
            } else first=current;
            records.push_back(current);
        }
    }
    // Indexing skips represented columns not used by this input. Preparation
    // still reports the additional retained data.
    {
        const auto source=triangle();local::LinearStalkBundle b(source,257);
        local::LinearCellularSheaf s0(b),s1(b);
        M base(257,257);base.set(0,0,2);base.set(256,256,3);
        M extended=base;
        for(std::size_t j=1;j<128;++j) extended.set(j,j,Q{5,7});
        s0.set_restriction(g(1),0,base);s1.set_restriction(g(1),0,extended);
        const A a0{O(s0)},a1{O(s1)};
        const auto x0=a0.bind(g(0),{{0,S(257,{{0,1},{256,2}})}});
        const auto x1=a1.bind(g(0),{{0,S(257,{{0,1},{256,2}})}});
        check(a0.coboundary(x0).work==a1.coboundary(x1).work,"action scanned irrelevant restriction columns");
        check(a1.preparation().indexed_terms>a0.preparation().indexed_terms,"preparation omitted stored map coefficients");
    }
    const A small(fixture(0,2,1)), large(fixture(0,2,64));
    const auto s=small.coboundary(small.bind(g(0),{{0,S::basis(2,1)}}));
    const auto l=large.coboundary(large.bind(g(0),{{0,S::basis(2,1)}}));
    check(s.work==l.work,"action scans unrelated source components");
    check(large.preparation().cells>small.preparation().cells,"preparation receipt hides additional source cells");
    for(std::size_t n:{std::size_t{2},std::size_t{257}}) {
        const O o=fixture(0,n);
        check(o.sheaf().restriction(g(1),0).nonzeros()==n,"bounded explicit identity reference");
        check(o.sheaf().coboundary(g(0)).nonzeros()==6*n,"bounded whole-map reference");
        const A action(o);
        check(action.preparation().indexed_terms==0,"default identity was numerically indexed");
    }
}
void ownership_and_limits() {
    const O owner=fixture(0);
    const A action(owner),same_owner(owner),foreign(fixture(0));
    const auto input=action.bind(g(0),{{0,S::basis(2,1)}});
    check(same(action.coboundary(input).field,same_owner.coboundary(input).field),"same owner reuse failed");
    rejects<std::invalid_argument>([&]{(void)foreign.coboundary(input);},"foreign owner accepted");
    rejects<std::out_of_range>([&]{(void)action.bind(g(8));},"foreign grade accepted");
    rejects<std::out_of_range>([&]{(void)action.bind(g(0),{{99,S(2)}});},"zero value bypassed source identity");
    rejects<std::invalid_argument>([&]{(void)action.bind(g(0),{{0,S(3)}});},"zero value bypassed stalk dimension");
    rejects<std::out_of_range>([&]{(void)action.coboundary(action.bind(g(2)));},"top grade needs explicit zero-carrier convention");
    rejects<std::out_of_range>([&]{(void)action.two_step_defect(action.bind(g(1)));},"defect lacks two retained grades");

    auto source=triangle();local::LinearStalkBundle bundle(source,2);local::LinearCellularSheaf mutable_sheaf(bundle);
    const A frozen{O(mutable_sheaf)};
    const auto saved=frozen.coboundary(frozen.bind(g(0),{{0,S::basis(2,0)}}));
    mutable_sheaf.set_restriction(g(1),0,rotation());bundle.set_dimension({g(0),0},3);
    check(same(saved.field,frozen.coboundary(frozen.bind(g(0),{{0,S::basis(2,0)}})).field),"caller mutation changed frozen owner");
    rejects<std::invalid_argument>([&]{A invalid{O(mutable_sheaf)};(void)invalid;},"unequal implicit restriction admitted");
    auto changed=triangle();local::LinearStalkBundle cb(changed,2);local::LinearCellularSheaf cs(cb);
    cb.set_dimension({g(0),0},3);
    rejects<std::invalid_argument>([&]{A invalid{O(cs)};(void)invalid;},"missing unequal restriction accepted");

    auto limits=A::default_limits();limits.steps=1;
    rejects<std::length_error>([&]{A limited(owner,limits);(void)limited;},"preparation step budget ignored");
    limits=A::default_limits();limits.dimension=1;
    rejects<std::length_error>([&]{A limited(owner,limits);(void)limited;},"dimension admission ignored");
    limits=A::default_limits();limits.entries=1;
    rejects<std::length_error>([&]{A limited(fixture(4),limits);(void)limited;},"index entry budget ignored");
    const A entry_limited(owner,limits);
    const auto one=entry_limited.bind(g(0),{{0,S::basis(2,0)}});
    rejects<std::length_error>([&]{(void)entry_limited.coboundary(one);},"action entry budget ignored");
    limits=A::default_limits();limits.steps=16;
    const A step_limited(owner,limits);
    rejects<std::length_error>([&]{(void)step_limited.coboundary(step_limited.bind(g(0),
        {{0,S(2,{{0,1},{1,2}})}}));},"action step budget ignored");
    limits.steps=32;
    const A composed_limited(owner,limits);
    const auto composed_input=composed_limited.bind(g(0),{{0,S(2,{{0,1},{1,2}})}});
    check(!composed_limited.coboundary(composed_input).field.zero(),"first composition step should fit");
    rejects<std::length_error>([&]{(void)composed_limited.two_step_defect(composed_input);},"two-step action reset its cumulative budget");
    limits=A::default_limits();limits.integer_bits=3;
    const A bit_limited(owner,limits);
    rejects<std::length_error>([&]{(void)bit_limited.bind(g(0),{{0,S::basis(2,0,9)}});},"input bit limit ignored");
    auto numeric_source=triangle();local::LinearStalkBundle nb(numeric_source,2);local::LinearCellularSheaf ns(nb);
    ns.set_restriction(g(1),0,M::from_rows({{7,0},{0,7}}));
    const A multiply_limited(O(ns),limits);
    rejects<std::length_error>([&]{(void)multiply_limited.coboundary(multiply_limited.bind(g(0),{{0,S::basis(2,0,7)}}));},"product bit growth unbounded");
    check(same(saved.field,frozen.coboundary(frozen.bind(g(0),{{0,S::basis(2,0)}})).field),"failure mutated an existing field");
}
} // namespace

int main() {
    try {
        compact_oracles();occurrences_and_direction();scaling();ownership_and_limits();
        std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks
                 <<",\"reference_fields\":"<<oracle_fields<<",\"reference_coordinates\":"<<exact_coordinates
                 <<",\"scaling\":[";
        for(std::size_t i=0;i<records.size();++i) {
            const auto& r=records[i];if(i)std::cout<<',';
            std::cout<<"{\"dimension\":"<<r.dimension<<",\"mode\":\""<<(r.mode?"explicit":"identity")
                     <<"\",\"index_entries\":"<<r.prep.retained_index_entries<<",\"preparation_steps\":"<<r.prep.checked_steps
                     <<",\"incidences\":"<<r.up.incidences<<",\"products\":"<<r.up.scalar_products
                     <<",\"action_steps\":"<<r.up.checked_steps<<",\"peak_live_entries\":"<<r.up.peak_live_numeric_entries
                     <<",\"defect_steps\":"<<r.defect.checked_steps<<'}';
        }
        std::cout<<"]}\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
