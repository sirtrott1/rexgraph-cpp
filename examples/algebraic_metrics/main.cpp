#include "model.hpp"
#include <iostream>

using namespace metric_example;
static void rational(const Q& value) { std::cout<<'"'<<value.get_str()<<'"'; }
static void qs(const rfc::numeric::RationalVector& values) {
    std::cout<<'[';bool first=true;for (const auto& value:values) {if (!first) std::cout<<',';first=false;rational(value);}std::cout<<']';
}
static void element(const E& value) { qs(value.coordinates()); }
static void elements(const std::vector<E>& values) {
    std::cout<<'[';bool first=true;for (const auto& value:values) {if (!first) std::cout<<',';first=false;element(value);}std::cout<<']';
}
static void matrix(const rfc::numeric::ExactMatrix& value) {
    std::cout<<'[';for (std::size_t i=0;i<value.rows();++i) {if (i) std::cout<<',';std::cout<<'[';
        for (std::size_t j=0;j<value.cols();++j) {if (j) std::cout<<',';rational(value.get(i,j));}std::cout<<']';}std::cout<<']';
}
static void vectors(const rfc::field::AlgebraicFiberMetricField::Values& values) {
    std::cout<<'[';bool first=true;for (const auto& value:values) {if (!first) std::cout<<',';first=false;elements(value.coordinates());}std::cout<<']';
}
static void profile(const Case& sample,bool deformed=false) {
    const auto& domain=sample.domain; const auto& f=domain.field();
    const auto model=cellular_example::make(f,true,false,false,Q{1},deformed);
    const auto prepared=model.prepare(deformed?rfc::local::AlgebraicChainPolicy::DeclaredDeformation:rfc::local::AlgebraicChainPolicy::RequireFlat);
    const auto field=metric_field(domain,prepared.realization());
    const auto u=model.bundle.stalk({G{0},0}),v=model.bundle.stalk({G{0},1});
    const auto& mu=field.metric(u.cell());const auto& mv=field.metric(v.cell());
    const auto one=f.scalar(Q{1}),zero=f.scalar(Q{0}),alpha=f.generator();
    const auto a=Map::declare(u,v,{one,alpha,one+alpha,zero});
    const auto x=u.vector({one+alpha,alpha*alpha}),y=v.vector({alpha,f.scalar(Q{2})});
    const auto solution=mu.solve(x);const auto adjoint=rfc::field::algebraic_metric_adjoint(a,mu,mv);
    std::cout<<"{\"phase\":\"metric\",\"name\":\""<<sample.name<<"\",\"deformed\":"<<(deformed?"true":"false")<<",\"polynomial\":";qs(f.minimal_polynomial());
    std::cout<<",\"involution\":";matrix(domain.involution().action());
    const auto probe=domain.is_real()?alpha-f.scalar(Q{5,4}):
        (f.degree()==2?one:alpha+domain.involution().apply(alpha)-f.scalar(f.degree()==4?Q{7,5}:Q{9,5}));
    const auto sign=domain.certify_sign(probe);
    std::cout<<",\"fixed_probe\":{\"value\":";element(probe);std::cout<<",\"interval\":[";
    rational(sign.interval.lower);std::cout<<',';rational(sign.interval.upper);
    std::cout<<"],\"refinements\":"<<sign.refinements<<",\"sign\":"<<sign.sign<<"},\"root\":";
    std::visit([&](const auto& root) {
        if constexpr (std::same_as<std::decay_t<decltype(root)>,rfc::geometry::SelectedRealAlgebraicEmbedding>) {
            const auto before=root.certificate().interval;const auto refined=root.refined_interval(sign.refinements);
            std::cout<<"{\"kind\":\"real\",\"interval\":[";rational(before.lower);std::cout<<',';rational(before.upper);
            std::cout<<"],\"refined\":[";rational(refined.lower);std::cout<<',';rational(refined.upper);std::cout<<"]}";
        } else {
            const auto before=root.certificate().region;const auto refinement=root.refine(sign.refinements);
            const auto diamond=[](const auto& region) {std::cout<<"{\"center\":[";rational(region.center.real);std::cout<<',';
                rational(region.center.imaginary);std::cout<<"],\"radius\":";rational(region.radius);std::cout<<'}';};
            std::cout<<"{\"kind\":\"complex\",\"region\":";diamond(before);std::cout<<",\"refined\":";diamond(refinement.region());std::cout<<'}';
        }
    },domain.embedding());
    std::cout<<",\"source_form\":";elements(mu.measurement().coefficient_form().coefficients());
    std::cout<<",\"target_form\":";elements(mv.measurement().coefficient_form().coefficients());
    std::cout<<",\"lower\":";elements(mu.lower_factor());
    std::cout<<",\"pivots\":[";
    for (std::size_t i=0;i<mu.pivot_certificates().size();++i) {
        if (i) {std::cout<<',';}const auto& p=mu.pivot_certificates()[i];
        std::cout<<"{\"value\":";element(p.value);std::cout<<",\"interval\":[";rational(p.interval.lower);std::cout<<',';
        rational(p.interval.upper);std::cout<<"],\"refinements\":"<<p.refinements<<",\"sign\":"<<p.sign<<'}';
    }
    std::cout<<"],\"map\":";elements(a.coefficients());std::cout<<",\"adjoint\":";elements(adjoint.coefficients());
    std::cout<<",\"x\":";elements(x.coordinates());std::cout<<",\"y\":";elements(y.coordinates());
    std::cout<<",\"solve\":";elements(solution.coordinates());std::cout<<",\"pairing\":";element(mu.inner(x,x));
    std::cout<<",\"rational_trace_form\":";matrix(mu.measurement().rational_trace_form());
    std::cout<<",\"grades\":[";
    const auto& source=model.bundle.source();
    for (std::size_t k=0;k<source.grade_count();++k) {
        if (k) {std::cout<<',';}const auto grade=G{static_cast<std::uint16_t>(k)};const auto input=values(field,grade);
        std::cout<<"{\"rank\":[";
        for (std::size_t i=0;i<input.size();++i) {if (i) std::cout<<',';std::cout<<input[i].carrier().rank();}
        std::cout<<"],\"forms\":[";
        for (std::size_t i=0;i<input.size();++i) {if (i) std::cout<<',';elements(field.metric({grade,static_cast<std::uint32_t>(i)}).measurement().coefficient_form().coefficients());}
        std::cout<<"],\"input\":";vectors(input);std::cout<<",\"hodge\":";vectors(field.hodge(grade,input));
        std::cout<<",\"quadrance\":";element(field.adjacent_quadrance(grade,input));std::cout<<",\"legs\":[";
        if (k) {
            const auto& arena=source.grade(grade);
            for (std::size_t i=0;i<arena.incidence_count();++i) {
                if (i) {std::cout<<',';}std::cout<<"{\"upper\":"<<arena.upper_index[i]<<",\"lower\":"<<arena.lower_index[i];
                std::cout<<",\"active\":"<<(arena.aspects.get(source.signature().existence_aspect(),i)?"true":"false")<<",\"weight\":";
                rational(source.incidence_coefficient(grade,i));std::cout<<",\"map\":";elements(field.system().occurrence_map(grade,i).coefficients());
                std::cout<<",\"adjoint\":";elements(field.occurrence_adjoint(grade,i).coefficients());std::cout<<'}';
            }
        }
        std::cout<<"]}";
    }
    std::cout<<"]}\n";
}
int main() {const auto profiles=cases();for (const auto& sample:profiles) profile(sample);profile(profiles[2],true);}
