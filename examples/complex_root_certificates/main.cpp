#include "model.hpp"
#include <iostream>

using namespace complex_example;
using Z=rfc::geometry::ExactComplexCoordinates;
using Diamond=rfc::geometry::ExactComplexDiamond;
static void q(const Q& value) {std::cout<<'"'<<value<<'"';}
static void point(const Z& value) {std::cout<<'[';q(value.real);std::cout<<',';q(value.imaginary);std::cout<<']';}
static void region(const Diamond& value) {std::cout<<"{\"center\":";point(value.center);std::cout<<",\"radius\":";q(value.radius);std::cout<<'}';}
static void coefficients(const std::vector<Z>& values) {
    std::cout<<'[';for (std::size_t i=0;i<values.size();++i) {if(i) std::cout<<',';point(values[i]);}std::cout<<']';
}
static void polynomial(const rfc::numeric::RationalVector& values) {
    std::cout<<'[';for (std::size_t i=0;i<values.size();++i) {if(i) std::cout<<',';q(values[i]);}std::cout<<']';
}
static void emit(const Case& sample) {
    const auto& embedding=sample.embedding;const auto& proof=embedding.certificate();const auto refined=embedding.refine(16);
    std::cout<<"{\"case\":\""<<sample.name<<"\",\"degree\":"<<sample.field.degree()<<",\"polynomial\":";
    polynomial(sample.field.minimal_polynomial());std::cout<<",\"region\":";region(proof.region);
    std::cout<<",\"taylor\":";coefficients(proof.taylor_coefficients);
    std::cout<<",\"fixed\":";coefficients(proof.fixed_point_coefficients);
    std::cout<<",\"beta\":";q(proof.center_step_bound);std::cout<<",\"contraction\":";q(proof.contraction_bound);
    std::cout<<",\"selfmap\":";q(proof.selfmap_bound);std::cout<<",\"margin\":";q(proof.margin);
    std::cout<<",\"root_count\":"<<proof.root_count<<",\"steps\":[";
    for (std::size_t i=0;i<refined.steps().size();++i) {
        if (i) std::cout<<',';
        const auto& step=refined.steps()[i];
        std::cout<<"{\"before\":";region(step.before);std::cout<<",\"after\":";region(step.after);
        std::cout<<",\"image\":";point(step.local_image);std::cout<<",\"slack\":";q(step.slack);
        std::cout<<",\"rounding_error\":";q(step.rounding_error);std::cout<<",\"grid_bits\":"<<step.grid_bits<<'}';
    }
    std::cout<<"],\"refined\":";region(refined.region());std::cout<<",\"values\":[";
    const auto alpha=sample.field.generator();
    const std::vector<rfc::numeric::AlgebraicElement> values{sample.field.scalar(0),sample.field.scalar(Q{7,3}),alpha,
        alpha+sample.field.scalar(1),alpha*alpha,alpha.inverse(),(alpha+sample.field.scalar(1)).power(3)};
    for (std::size_t i=0;i<values.size();++i) {
        if (i) std::cout<<',';
        const auto observed=embedding.observe(values[i]);
        std::cout<<"{\"coordinates\":";polynomial(values[i].coordinates());std::cout<<",\"enclosure\":";
        region(observed.enclosure(refined));std::cout<<",\"zero\":"<<(observed.is_zero()?"true":"false")<<'}';
    }
    std::cout<<"]}\n";
}
int main() {
    try {
        const auto samples=cases();for (const auto& sample:samples) emit(sample);
        const auto source=samples[0],target=samples[4],last=samples[10];
        const auto first=rfc::numeric::AlgebraicFieldEmbedding::declare(source.field,target.field,target.field.generator().power(2));
        const auto second=rfc::numeric::AlgebraicFieldEmbedding::declare(target.field,last.field,last.field.generator().power(2));
        const auto composite=first.then(second);
        std::cout<<"{\"case\":\"comparisons\",\"quadratic_to_quartic\":"<<(source.embedding.agrees_with(first,target.embedding)?"true":"false")
                 <<",\"opposite_root\":"<<(source.embedding.agrees_with(first,target.embedding.conjugated())?"true":"false")
                 <<",\"quartic_to_octic\":"<<(target.embedding.agrees_with(second,last.embedding)?"true":"false")
                 <<",\"composite\":"<<(source.embedding.agrees_with(composite,last.embedding)?"true":"false")<<"}\n";
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
