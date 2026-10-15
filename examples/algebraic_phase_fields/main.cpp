#include "rfc/core/builder.hpp"
#include "rfc/geometry/quadratic_embedding.hpp"
#include "rfc/topology/algebraic_attachment.hpp"
#include <iostream>

using Q=rfc::numeric::Rational;
static void vector(const rfc::numeric::RationalVector& values) {
    std::cout<<'[';
    for(std::size_t i=0;i<values.size();++i) {if(i)std::cout<<',';std::cout<<'"'<<values[i]<<'"';}
    std::cout<<']';
}
static void matrix(const rfc::numeric::ExactMatrix& value) {
    std::cout<<'[';
    for(std::size_t i=0;i<value.rows();++i) {
        if(i)std::cout<<',';
        rfc::numeric::RationalVector row(value.cols());
        for(std::size_t j=0;j<value.cols();++j)row[j]=value.get(i,j);
        vector(row);
    }
    std::cout<<']';
}
static rfc::core::RelationalComplex trefoil() {
    using namespace rfc;
    core::RelationSignature signature;
    const auto edge=signature.add_type("meridian",core::BoundaryPolicy::DeclaredExact,2,0);
    const auto face=signature.add_attaching_loop_type("attaching_word",1,0);
    core::RelationalComplexBuilder builder(signature);
    for(int i=0;i<2;++i)builder.add_grade_one(edge,{{7,true,true,false,1},{7,true,false,false,1}});
    std::vector<core::HigherIncidenceInput> occurrences;
    for(const int letter:{1,2,1,-2,-1,-2})
        occurrences.emplace_back(static_cast<std::uint32_t>((letter>0?letter:-letter)-1),true,letter<0,false,Q{1});
    builder.add_higher(core::Grade{2},face,occurrences);return builder.freeze();
}
int main() {
    using namespace rfc;
    const auto real=numeric::CertifiedAlgebraicField::declare({-2,0,1},numeric::EisensteinShift{2,0});
    const auto real_root=geometry::SelectedQuadraticEmbedding::real_root(real,geometry::QuadraticRealRoot::Larger,1,2);
    const auto real_coordinates=real_root.coordinates(real.generator());
    const auto inverse=(real.scalar(Q{1})+real.generator()).inverse();
    std::cout<<"{\"case\":\"positive_sqrt2\",\"minimal_polynomial\":";
    vector(real.minimal_polynomial());
    std::cout<<",\"interval\":[\"1\",\"2\"],\"radicand\":\""<<real_coordinates.radicand
             <<"\",\"inverse_one_plus_root\":";vector(inverse.coordinates());std::cout<<"}\n";
    const topology::PreparedCyclicAttachingRealization formal(trefoil());
    for(const bool sixth:{true,false}) {
        const auto field=numeric::CertifiedAlgebraicField::declare({1,sixth?-1:1,1},numeric::IrreducibleReduction{2});
        const auto root=geometry::SelectedQuadraticEmbedding::complex_root(field,geometry::QuadraticHalfPlane::Upper);
        const numeric::AlgebraicUnitPhase phase(root.conjugation(),field.generator());
        const auto selected=topology::PreparedAlgebraicAttachingRealization::common_generator(
            formal,phase.value(),topology::AttachingTransportConvention::ChronologicalColumns);
        const auto coordinates=root.phase_coordinates(phase);const auto h=selected.homology_dimensions_over_field();
        std::cout<<"{\"case\":\""<<(sixth?"sixth_root":"third_root")<<"\",\"minimal_polynomial\":";
        vector(field.minimal_polynomial());
        std::cout<<",\"half_plane\":\"upper\",\"rational_part\":\""<<coordinates.rational_part
                 <<"\",\"radical_coefficient\":\""<<coordinates.radical_coefficient
                 <<"\",\"radicand\":\""<<coordinates.radicand<<"\",\"rational_action\":";
        matrix(phase.regular_action());std::cout<<",\"norm_form\":";matrix(root.complex_norm_form());
        std::cout<<",\"formal_derivative_nonzero\":"<<(!formal.faces()[0].derivatives[0].is_zero()?"true":"false")
                 <<",\"specialized_derivative_zero\":"<<(selected.faces()[0].derivatives[0].is_zero()?"true":"false")
                 <<",\"homology_over_field\":["<<h[0]<<','<<h[1]<<','<<h[2]<<"]}\n";
    }
    const auto fifth=numeric::CertifiedAlgebraicField::declare({1,1,1,1,1},numeric::IrreducibleReduction{2});
    const auto involution=numeric::AlgebraicInvolution::declare(fifth,fifth.generator().inverse());
    const numeric::AlgebraicUnitPhase phase(involution,fifth.generator());
    const auto fifth_power=phase.power(5);
    std::cout<<"{\"case\":\"fifth_root_regular_action\",\"minimal_polynomial\":";
    vector(fifth.minimal_polynomial());std::cout<<",\"rational_dimension\":"<<fifth.degree()<<",\"action\":";
    matrix(phase.regular_action());std::cout<<",\"trace_form\":";matrix(involution.trace_form());
    std::cout<<",\"fifth_power_is_identity\":"<<(fifth_power.value()==fifth.scalar(Q{1})?"true":"false")<<"}\n";
}
