#include "rfc/core/axioms.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/geometry/rational_rotation.hpp"
#include "rfc/local/prepared_field_action.hpp"
#include "rfc/storage/rfcx.hpp"
#include "rfc/topology/attaching_transport.hpp"
#include "rfc/topology/cyclic_attachment.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using Q = rfc::numeric::Rational;
using M = rfc::numeric::ExactMatrix;
using V = rfc::numeric::RationalVector;
using G = rfc::core::Grade;
using P = rfc::topology::PreparedAttachingTransport;
using Map = rfc::topology::GeneratorTransport;
constexpr auto convention = rfc::topology::AttachingTransportConvention::ChronologicalColumns;
static std::size_t checks{};
template<class Dimension> concept AcceptedDimension = requires(const rfc::core::RelationalComplex& value,
                                                              const std::vector<Map>& transports) {
    P::common_rational_coordinates(value,Dimension{},transports,convention);
};
static_assert(!AcceptedDimension<double> && AcceptedDimension<std::size_t>);
static void require(bool value,const char* reason) {
    if (!value) throw std::runtime_error(reason);
    ++checks;
}
template<class Exception,class Action> static void rejects(Action action,const char* reason) {
    bool rejected = false;
    try { action(); } catch (const Exception&) { rejected = true; }
    require(rejected,reason);
}
static rfc::core::RelationalComplex source(std::size_t generators,const std::vector<std::vector<int>>& words,
                                          bool inactive = false,Q endpoint_magnitude = Q{1}) {
    rfc::core::RelationSignature signature;
    const auto loop = signature.add_type("meridian",rfc::core::BoundaryPolicy::DeclaredExact,2,0);
    const auto face = signature.add_attaching_loop_type("attaching_word",1,0);
    rfc::core::RelationalComplexBuilder builder(signature);
    for (std::size_t g = 0; g < generators; ++g) {
        std::vector<rfc::core::BoundaryOccurrenceInput> endpoints{{99,true,true,false,endpoint_magnitude},
                                                     {99,true,false,false,endpoint_magnitude}};
        if (inactive) endpoints.push_back({99,false,false,false,7});
        builder.add_grade_one(loop,endpoints);
    }
    builder.declare_grade(G{2});
    for (const auto& word : words) {
        std::vector<rfc::core::HigherIncidenceInput> occurrences;
        for (const auto letter : word) {
            if (inactive) occurrences.emplace_back(0,false,false,false,Q{7});
            occurrences.emplace_back(static_cast<std::uint32_t>((letter > 0 ? letter : -letter)-1),
                                     true,letter < 0,false,Q{1});
        }
        builder.add_higher(G{2},face,occurrences);
    }
    return builder.freeze();
}
static std::vector<Map> maps(const std::vector<M>& values) {
    std::vector<Map> result;
    for (std::size_t g = 0; g < values.size(); ++g)
        result.push_back({{G{1},static_cast<std::uint32_t>(g)},values[g]});
    return result;
}
static M independent_word_action(const std::vector<int>& word,const std::vector<M>& values) {
    auto result = M::identity(values[0].rows());
    // Apply to every basis column, independently of the prepared matrix walk.
    for (std::size_t j = 0; j < result.cols(); ++j) {
        V vector(result.rows(),Q{0}); vector[j] = 1;
        for (const auto letter : word) {
            const auto index = static_cast<std::size_t>((letter > 0 ? letter : -letter)-1);
            vector = (letter > 0 ? values[index] : values[index].inverse()).apply(vector);
        }
        for (std::size_t i = 0; i < vector.size(); ++i) result.set(i,j,vector[i]);
    }
    return result;
}
static void check_word(const std::vector<int>& word,const std::vector<M>& values) {
    const auto original = source(values.size(),{word},true);
    const auto image = rfc::storage::encode_rfcx(original).bytes;
    const auto prepared = P::common_rational_coordinates(original,values[0].rows(),maps(values),convention);
    require(rfc::storage::encode_rfcx(prepared.source()).bytes == image,"complete source canonical bytes retained");
    require(rfc::core::bianchi_through(prepared.source(),G{2}).satisfied(),"underlying source remains flat");
    const auto H = independent_word_action(word,values);
    require(prepared.faces()[0].holonomy == H,"chronological action matches independent basis application");
    require(prepared.curvature() == H.add(M::identity(H.rows()).scale(Q{-1})),"complete holonomy defect");
    require(prepared.boundary_one().multiply(prepared.boundary_two()) == prepared.curvature(),"noncommuting boundary product");
    M routes(H.rows(),H.cols());
    for (const auto& term : prepared.faces()[0].occurrences) {
        const auto difference = values[term.generator.index].add(M::identity(H.rows()).scale(Q{-1}));
        require(difference.multiply(term.contribution) == term.boundary_contribution,"every route retains its noncommuting factors");
        routes = routes.add(term.boundary_contribution);
        require(prepared.fibers().flag_map(G{2},term.source_slot).transport == term.contribution,"realized fiber map used once");
        if (!term.active) require(term.contribution.nonzeros() == 0 && term.boundary_contribution.nonzeros() == 0,"inactive occurrence retained with zero route");
    }
    require(routes == prepared.curvature(),"all occurrence routes assemble to curvature");
    if (prepared.flat()) {
        const auto dimensions = prepared.homology_dimensions();
        require(dimensions[0]+dimensions[2] == dimensions[1],"exact Euler characteristic for one face and two generators");
    } else rejects<std::domain_error>([&]{ (void)prepared.homology_dimensions(); },"curved homology refusal");
}
static void ordered_properties() {
    const M A = M::from_rows({{1,1},{0,1}}), B = M::from_rows({{1,0},{1,1}});
    require(A.multiply(B) != B.multiply(A),"test maps do not commute");
    // Every word through length four, including inverses and cancellations.
    const std::array<int,4> alphabet{1,2,-1,-2};
    for (std::size_t length = 1, count = 4; length <= 4; ++length,count *= 4) {
        for (std::size_t code = 0; code < count; ++code) {
            auto remaining = code;
            std::vector<int> word(length);
            for (auto& letter : word) { letter = alphabet[remaining % 4]; remaining /= 4; }
            check_word(word,{A,B});
        }
    }
    const auto quarter = rfc::numeric::RationalPhase(Q{0},Q{1});
    const auto X = rfc::geometry::coordinate_rotation(rfc::geometry::CoordinateAxis::X,quarter);
    const auto Y = rfc::geometry::coordinate_rotation(rfc::geometry::CoordinateAxis::Y,quarter);
    check_word({1,2,-1,-2},{X,Y});
    check_word({1,-1,2,-2},{X,Y});
}
static void integration_and_ownership() {
    using namespace rfc;
    const M A = M::from_rows({{1,1},{0,1}}), B = M::from_rows({{1,0},{-1,1}});
    const std::vector<int> trefoil{1,2,1,-2,-1,-2};
    auto original = source(2,{trefoil},true,Q{3});
    auto declared = maps({A,B});
    const auto prepared = P::common_rational_coordinates(original,2,declared,convention);
    require(prepared.flat() && A != M::identity(2),"flat local attachments retain nontrivial global generator transport");
    require(&prepared.source() != &original && &prepared.fibers().complex() == &prepared.source(),"fiber carrier pins its actual retained source");
    const auto copy = prepared;
    const auto separate = P::common_rational_coordinates(original,2,declared,convention);
    require(prepared.same_owner(copy) && !prepared.same_owner(separate),"equal inputs do not identify owners");
    original = source(1,{}); declared[0].forward = M::identity(2);
    require(prepared.generators()[0].forward == A && prepared.source().grade(G{1}).cell_count() == 2,"caller replacement cannot mutate prepared source or maps");
    require(prepared.source().incidence_coefficient(G{1},0) == -3,"original endpoint weights remain retained under explicit realized maps");
    const topology::PairwisePath path(prepared.source(),0,{{0,true},{1,true},{0,false}});
    require(prepared.path_transport(path) == A.inverse().multiply(B).multiply(A),"path column convention");
    require(prepared.path_transport(path.inverse()).multiply(prepared.path_transport(path)) == M::identity(2),"reverse path uses actual inverse");
    const topology::PairwisePath foreign(separate.source(),0,{{0,true}});
    rejects<std::invalid_argument>([&]{ (void)prepared.path_transport(foreign); },"foreign path owner refused");
    rejects<std::logic_error>([&]{ (void)prepared.path_transport(topology::PairwisePath{}); },"unbound path rejected before source access");
    const auto presentation = topology::fundamental_groupoid_presentation(prepared.source());
    const auto extended = topology::apply_face_homotopy(presentation,path,{0,2,true,true,1});
    require(prepared.path_transport(extended) == prepared.path_transport(path),"certified flat face move preserves transport");
    const local::LinearStalkBundle stalks(prepared.source(),2);
    local::LinearCellularSheaf sheaf(stalks);
    for (const auto& generator : prepared.generators()) {
        const auto [tail,head] = topology::pairwise_incidence_slots(prepared.source(),generator.generator.index);
        require(tail != head,"self-loop endpoint occurrences stay distinct");
        sheaf.set_restriction(G{1},tail,generator.forward);
        sheaf.set_restriction(G{1},head,M::identity(2));
    }
    require(topology::local_system_transport(sheaf,path) == prepared.path_transport(path),"native sheaf transport agrees on loops and noncommuting steps");
    const local::PreparedGradeFieldAction action(prepared.fibers(),G{2});
    auto workspace = action.make_workspace();
    const V input{2,3};
    require(action.apply(input,workspace) == prepared.boundary_two().apply(input),"existing native field action consumes new occurrence maps");
    const auto base_two = M::from_rows({{1,0},{0,1},{-1,0},{0,-1}});
    const auto deformation_two = prepared.boundary_two().add(base_two.scale(Q{-1}));
    const auto mixed = prepared.boundary_one().multiply(base_two);
    require(mixed.nonzeros() != 0 && mixed.add(prepared.boundary_one().multiply(deformation_two)).nonzeros() == 0,
            "complete noncommuting deformation retains and cancels mixed terms");
    const auto retained = [] {
        const auto source_value = source(2,{{1,2,-1,-2}});
        return P::common_rational_coordinates(source_value,2,maps({M::from_rows({{1,1},{0,1}}),M::from_rows({{1,0},{1,1}})}),convention);
    }();
    require(retained.faces().size() == 1 && !retained.flat(),"prepared analysis survives input lifetime");
    const auto family = P::common_rational_coordinates(source(2,{trefoil,{1,2,-1,-2}}),2,maps({A,B}),convention);
    require(family.faces()[0].curvature.nonzeros() == 0 && !family.flat(),"later face defect included in complete family");
    const auto cyclic_source = source(2,{trefoil},true);
    const topology::PreparedCyclicAttachingRealization cyclic(cyclic_source);
    for (const auto& T : {M::identity(2),M::from_rows({{0,-1},{1,1}}),A}) {
        const auto legacy = cyclic.specialize_common_transport(T);
        const auto general = P::common_rational_coordinates(cyclic_source,2,maps({T,T}),convention);
        require(legacy.boundary_one() == general.boundary_one() && legacy.boundary_two() == general.boundary_two() && legacy.curvature() == general.curvature(),"shared cyclic specialization remains exact");
    }
    const auto cancel = P::common_rational_coordinates(source(2,{{1,-1,2,-2}}),2,maps({A,B}),convention);
    require(cancel.flat() && cancel.boundary_two().nonzeros() == 0 && cancel.faces()[0].occurrences[0].boundary_contribution.nonzeros() != 0,"supported local routes can cancel in the complete interface");
    const auto circle = P::common_rational_coordinates(source(1,{}),2,maps({A}),convention);
    require(circle.flat() && circle.faces().empty() && circle.curvature().cols() == 0 &&
            circle.homology_dimensions() == std::array<std::size_t,3>{1,1,0},"empty face grade and nontrivial global circle transport retained");
    const auto reordered = P::common_rational_coordinates(prepared.source(),2,
        {{{G{1},1},B},{{G{1},0},A}},convention);
    require(reordered.boundary_two() == prepared.boundary_two(),"generator declarations bind by identity rather than input vector position");
}
static void covariance_and_admission() {
    using namespace rfc;
    const M A = M::from_rows({{1,1},{0,1}}), B = M::from_rows({{1,0},{1,1}}), S = M::from_rows({{2,1},{0,1}});
    const auto original = source(2,{{1,2,-1,-2}});
    const auto first = P::common_rational_coordinates(original,2,maps({A,B}),convention);
    const auto Si = S.inverse();
    const auto changed = P::common_rational_coordinates(original,2,maps({Si.multiply(A).multiply(S),Si.multiply(B).multiply(S)}),convention);
    require(changed.curvature() == Si.multiply(first.curvature()).multiply(S),"coherent common gauge changes conjugate curvature");
    const auto blockS = M::from_rows({{2,1,0,0},{0,1,0,0},{0,0,2,1},{0,0,0,1}});
    require(changed.boundary_one() == Si.multiply(first.boundary_one()).multiply(blockS) &&
            changed.boundary_two() == blockS.inverse().multiply(first.boundary_two()).multiply(S),
            "full adjacent maps obey coherent gauge changes");
    require(first.flat() == changed.flat(),"coordinate change cannot create flatness");
    const auto metric = S.transpose().multiply(S);
    const auto energy = first.curvature().transpose().multiply(first.curvature());
    const auto transported_energy = metric.inverse().multiply(changed.curvature().transpose()).multiply(metric).multiply(changed.curvature());
    require(energy.get(0,0)+energy.get(1,1) == transported_energy.get(0,0)+transported_energy.get(1,1),"explicitly transported metric preserves curvature quadrance");
    auto declared = maps({A,B});
    rejects<std::invalid_argument>([&]{ (void)P::common_rational_coordinates(original,2,{},convention); },"complete map count required");
    declared[1].generator = declared[0].generator;
    rejects<std::invalid_argument>([&]{ (void)P::common_rational_coordinates(original,2,declared,convention); },"duplicate map refused");
    declared = maps({A,B}); declared[1].generator.grade = G{2};
    rejects<std::invalid_argument>([&]{ (void)P::common_rational_coordinates(original,2,declared,convention); },"wrong generator grade refused");
    declared = maps({A,B}); declared[1].generator.index = 9;
    rejects<std::invalid_argument>([&]{ (void)P::common_rational_coordinates(original,2,declared,convention); },"foreign generator slot refused");
    rejects<std::invalid_argument>([&]{ (void)P::common_rational_coordinates(original,2,maps({A,M(1,2)}),convention); },"rectangular generator refused");
    rejects<std::runtime_error>([&]{ (void)P::common_rational_coordinates(original,2,maps({A,M(2,2)}),convention); },"singular generator refused");
    rejects<std::invalid_argument>([&]{ (void)P::common_rational_coordinates(original,0,maps({A,B}),convention); },"empty common carrier refused");
    rejects<std::invalid_argument>([&]{ (void)P::common_rational_coordinates(original,2,maps({A,B}),static_cast<topology::AttachingTransportConvention>(255)); },"unsupported convention refused");
    for (int kind = 0; kind < 4; ++kind) {
        topology::AttachingTransportLimits limits;
        if (kind == 0) limits.max_cells = 0;
        if (kind == 1) limits.max_occurrences = 0;
        if (kind == 2) limits.max_transport_dimension = 1;
        if (kind == 3) limits.max_retained_coordinate_slots = 0;
        rejects<std::length_error>([&]{ (void)P::common_rational_coordinates(original,2,maps({A,B}),convention,limits); },"explicit admission budget enforced");
    }
    topology::AttachingTransportLimits limits; limits.max_integer_bits = 1;
    rejects<std::length_error>([&]{ (void)P::common_rational_coordinates(original,2,maps({A,B}),convention,limits); },"growing noncommuting coefficients reject at the declared bit bound");
    limits.max_integer_bits = 0;
    rejects<std::invalid_argument>([&]{ (void)P::common_rational_coordinates(original,2,maps({A,B}),convention,limits); },"empty arithmetic budget refused");
    limits = {}; limits.max_occurrences = 8;
    const auto path_bound = P::common_rational_coordinates(original,2,maps({A,B}),convention,limits);
    const topology::PairwisePath long_path(path_bound.source(),0,std::vector<topology::EdgeStep>(9,{0,true}));
    rejects<std::length_error>([&]{ (void)path_bound.path_transport(long_path); },"path work admission enforced");
    core::RelationSignature signature;
    const auto loop = signature.add_type("loop",core::BoundaryPolicy::DeclaredExact,2,2);
    const auto face = signature.add_attaching_loop_type("weighted",1,0);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(loop,{{99,true,true,false,1},{99,true,false,false,1}});
    builder.add_higher(G{2},face,{{0,true,false,false,2}});
    const auto weighted = builder.freeze();
    rejects<std::invalid_argument>([&]{ (void)P::common_rational_coordinates(weighted,2,maps({A}),convention); },"nonunit face incidence cannot silently become a word step");
    builder.declare_grade(G{3});
    const auto extended = builder.freeze();
    rejects<std::invalid_argument>([&]{ (void)P::common_rational_coordinates(extended,2,maps({A}),convention); },"higher source grades require their own complete transport family");
}
int main() {
    try {
        ordered_properties(); integration_and_ownership(); covariance_and_admission();
        std::cout << "{\"checks\":" << checks << ",\"failed\":0,\"exhaustive_words\":340,\"domain\":\"exact rational noncommuting transports\"}\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "failure after " << checks << " checks: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
