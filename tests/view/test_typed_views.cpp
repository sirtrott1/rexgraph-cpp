#include "rfc/view/typed_accession.hpp"
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

static rfc::numeric::ExactMatrix selector(std::size_t ambient,const std::vector<std::size_t>& coords) {
    rfc::numeric::ExactMatrix A(coords.size(),ambient);
    for(std::size_t i=0;i<coords.size();++i) A.set(i,coords[i],rfc::numeric::Rational{1});
    return A;
}

int main() {
    try {
        using namespace rfc;
        using R=numeric::Rational;

        const auto Aa=selector(4,{0,1,2});
        const auto Ab=selector(4,{2,3});
        const auto Fa=Aa.transpose();
        const auto Fb=Ab.transpose();
        view::TypedAccession va("a",Aa,Fa), vb("b",Ab,Fb);
        field::ExactMetric M(numeric::ExactMatrix::diagonal({R{2},R{3},R{4},R{5}}));
        const numeric::RationalVector x{R{2},R{3},R{5},R{7}};
        require(va.observe(x)==numeric::RationalVector({R{2},R{3},R{5}}), "typed accession a mismatch");
        require(vb.observe(x)==numeric::RationalVector({R{5},R{7}}), "typed accession b mismatch");
        const auto cross=view::physical_cross_metric(va,vb,M);
        const auto ua=va.observe(x), ub=vb.observe(x);
        require(numeric::dot(ua,cross.apply(ub))==R{100}, "overlap physical co-relation should equal shared coordinate contribution");

        // R^a=S^{-1}F^a and R^b=S^{-1}F^b, where S=diag(1,1,2,1).
        const auto Sinv=numeric::ExactMatrix::diagonal({R{1},R{1},R{1,2},R{1}});
        const auto Ra=Sinv.multiply(Fa);
        const auto Rb=Sinv.multiply(Fb);
        view::ReconstructingTypedFamily family({
            view::ReconstructingTypedView(view::TypedAccession("a",Aa,Fa),Ra),
            view::ReconstructingTypedView(view::TypedAccession("b",Ab,Fb),Rb)});
        require(family.stacked_reconstruction().multiply(family.stacked_accession())==numeric::ExactMatrix::identity(4),
                "overlapping typed reconstruction does not satisfy RA=I");
        require(family.reconstruct(family.observe(x))==x,"typed redundant coordinates did not reconstruct ambient field");
        require(family.consistent(family.observe(x)),"ambient-derived redundant coordinates should lie in represented image");
        numeric::RationalVector bad=family.observe(x); bad[3]+=1;
        require(!family.consistent(bad),"inconsistent duplicate overlap coordinates were accepted");
        require(view::metric_pullback_certificate(family,M),"faithful represented metric does not pull back to ambient metric");
        const auto Mrep=view::represented_metric(family,M);
        const numeric::RationalVector duplicate_difference{R{0},R{0},R{1},R{-1},R{0}};
        require(numeric::dot(duplicate_difference,Mrep.apply(duplicate_difference))==0,
                "redundant overlap difference should be null in represented metric");

        // Represented boundary contraction: A0 B1 R1 followed by A1 B2 R2 preserves the chain law.
        const auto I4=numeric::ExactMatrix::identity(4);
        view::ReconstructingTypedFamily lower({view::ReconstructingTypedView(view::TypedAccession("all",I4,I4),I4)});
        // Reuse the overlapping family at grade one. Construct B1 and B2 from the nested paper fixture.
        numeric::ExactMatrix B1(4,4);
        B1.set(0,0,R{-1}); B1.set(1,0,R{1,3}); B1.set(2,0,R{1,3}); B1.set(3,0,R{1,3});
        B1.set(0,1,R{-1}); B1.set(1,1,R{1});
        B1.set(0,2,R{-1}); B1.set(2,2,R{1});
        B1.set(0,3,R{-1}); B1.set(3,3,R{1});
        numeric::ExactMatrix B2(4,1);
        B2.set(0,0,R{-1}); B2.set(1,0,R{1,3}); B2.set(2,0,R{1,3}); B2.set(3,0,R{1,3});
        require(B1.multiply(B2).rank()==0,"nested chain fixture invalid");
        const auto A2=numeric::ExactMatrix::identity(1);
        view::ReconstructingTypedFamily upper({view::ReconstructingTypedView(view::TypedAccession("face",A2,A2),A2)});
        const auto Bt1=view::represented_map(lower,B1,family);
        const auto Bt2=view::represented_map(family,B2,upper);
        require(Bt1.multiply(Bt2).rank()==0,"reconstructing typed contraction broke adjacent chain law");

        // The physical overlap operator F A double-counts shared coordinate and can break the contraction.
        const auto S=Fa.multiply(Aa).add(Fb.multiply(Ab));
        require(B1.multiply(S).multiply(B2).rank()!=0,"physical overlap double-counting should break this contraction fixture");

        // Exact descent criterion from the paper.
        numeric::ExactMatrix pairB(2,1); pairB.set(0,0,R{-1}); pairB.set(1,0,R{1});
        numeric::ExactMatrix A1(1,1); A1.set(0,0,R{1});
        numeric::ExactMatrix A0(1,2); A0.set(0,0,R{1}); A0.set(0,1,R{1});
        require(view::descent_certificate(A1,A0,pairB).descends,"valid quotient boundary did not descend");
        const auto induced=view::induced_effective_map(A1,A0,pairB);
        require(induced.map.rows()==1u && induced.map.cols()==1u && induced.map.get(0,0)==R{0},
                "induced effective quotient boundary should be zero");
        numeric::ExactMatrix Azero(0,1);
        numeric::ExactMatrix Aleft(1,2); Aleft.set(0,0,R{1});
        require(!view::descent_certificate(Azero,Aleft,pairB).descends,"invalid quotient boundary descent was accepted");

        // Deterministic overlapping-cover audit. Each coordinate appears in one
        // or more views; reconstruction divides its contributions by exact
        // multiplicity and must recover the ambient field and metric exactly.
        std::mt19937 rng(0x71E5u);
        for (std::size_t n=2;n<=8;++n) {
            const std::size_t view_count=2u+(rng()%4u);
            std::vector<std::vector<std::size_t>> coords(view_count);
            std::vector<std::size_t> multiplicity(n,0u);
            for (std::size_t j=0;j<n;++j) {
                const std::size_t first=rng()%view_count;
                coords[first].push_back(j); ++multiplicity[j];
                if ((rng()&1u)!=0u) {
                    const std::size_t second=(first+1u+(rng()%(view_count-1u)))%view_count;
                    coords[second].push_back(j); ++multiplicity[j];
                }
            }
            std::vector<view::ReconstructingTypedView> random_views;
            for (std::size_t v=0;v<view_count;++v) {
                const auto A=selector(n,coords[v]);
                const auto F=A.transpose();
                numeric::ExactMatrix Rm(n,coords[v].size());
                for(std::size_t local=0;local<coords[v].size();++local) {
                    const auto j=coords[v][local];
                    Rm.set(j,local,R{1,static_cast<long>(multiplicity[j])});
                }
                random_views.emplace_back(view::TypedAccession("v"+std::to_string(v),A,F),Rm);
            }
            view::ReconstructingTypedFamily random_family(std::move(random_views));
            numeric::RationalVector diag(n,R{0}), field_value(n,R{0});
            for(std::size_t j=0;j<n;++j) { diag[j]=R{static_cast<long>(j+2u)}; field_value[j]=R{static_cast<long>(2u*j+1u)}; }
            field::ExactMetric random_metric(numeric::ExactMatrix::diagonal(diag));
            require(random_family.reconstruct(random_family.observe(field_value))==field_value,
                    "random overlapping typed family failed exact reconstruction");
            require(view::metric_pullback_certificate(random_family,random_metric),
                    "random overlapping typed family failed exact metric pullback");
        }

        std::cout << "typed accession/reconstruction tests passed\n";
        return EXIT_SUCCESS;
    } catch(const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
