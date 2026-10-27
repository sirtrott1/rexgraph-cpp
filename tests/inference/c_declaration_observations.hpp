#pragma once
#include "rfc/inference/c_declaration_archive.hpp"
namespace c_declaration_observations {
using namespace rfc;
using namespace inference;
using namespace temporal;
using G = numeric::ExactActionGuard;
using Q = numeric::Rational;
using V = numeric::RationalVector;
using M = numeric::ExactMatrix;
static std::string q(const Q &value) {
    return value.get_str();
}
static std::string matrix(const M &m) {
    std::string out;
    for (std::size_t i = 0; i < m.rows(); ++i) {
        for (std::size_t j = 0; j < m.cols(); ++j)
            out += q(m.get(i, j)) + ",";
        out += ';';
    }
    return out;
}
static std::string vector(const V &v) {
    std::string out;
    for (const auto &x : v)
        out += q(x) + ",";
    return out;
}
static std::string observe(const std::string &name, const CDeclarationArchive &a, G &g) {
    auto e = a.exports();
    if (name == "characters") {
        const auto &c = a.get<topology::PreparedCyclicCharacterDecomposition>(e[0]);
        std::string out;
        for (auto n : c.homology_dimensions())
            out += std::to_string(n) + ",";
        return out + "sectors=" + std::to_string(c.sectors().size());
    }
    if (name == "branch")
        return matrix(a.get<local::PreparedBranchChain>(e[0]).basis());
    if (name == "periods") {
        const auto &p = a.get<field::PreparedIntegralPeriodConstruction>(e[0]);
        return matrix(p.lattice().gram()) + matrix(p.lattice().cup_form());
    }
    if (name == "surface-factors") {
        const auto &f = a.get<field::PreparedFiniteEllipticFactors>(e[0]);
        return f.certificate().index().get_str() + "," + q(f.certificate().factors()[0].intersection) + "," +
               q(f.certificate().factors()[0].squared_frequency);
    }
    if (name == "radical-normalization") {
        const auto &n = a.get<field::PreparedFiniteJacobianNormalization>(e[0]);
        return vector(n.sectors()[0].declaration.positive_root.coordinates()) + matrix(n.star_square());
    }
    if (name == "univariate") {
        const auto &f = a.get<geometry::PreparedUnivariateFamily>(e[0]);
        const auto &d = a.get<geometry::UnivariateDomain>(e[2]);
        auto at = d.at(Q{1}, g);
        return std::to_string(f.rank_at(at, g));
    }
    if (name == "jets-and-words") {
        const auto &r = a.get<PreparedResponseJet::Result>(e[0]);
        const auto &green = a.get<PreparedResponseJet::Result>(e[1]);
        const auto &word = a.get<PreparedDerivativeWord::Result>(e[2]);
        return vector(r.value()) + vector(r.dot()) + vector(green.value()) + vector(green.dot()) +
               vector(word.value()) + vector(word.dot());
    }
    if (name == "rational-family") {
        const auto &f = a.get<PreparedCovariantFamily>(e[0]);
        const auto &jet = a.get<PreparedCovariantJet>(e[1]);
        return matrix(jet.hodge()[0]) + "," + std::to_string(f.domain().contains(f.domain().at(Q{0}, g), g));
    }
    if (name == "holonomy") {
        const auto &i = a.get<PreparedSurfaceInheritance>(e[0]);
        std::string out;
        for (auto n : i.dimensions())
            out += std::to_string(n) + ",";
        return out;
    }
    if (name == "rotating") {
        const auto &r = a.get<PreparedRotatingCoefficient>(e[0]);
        auto h = r.half_turn(g);
        auto sample = r.sample(r.domain().at(Q{1}, g), g);
        return q(h.obstruction()) + "," + q(sample.obstruction()) + "," + vector(sample.solution_real()) +
               vector(sample.solution_imaginary());
    }
    if (name == "refinement") {
        const auto &energy = a.get<PreparedRefinementMap::EnergyReceipt>(e[0]);
        const auto &split = a.get<PreparedRefinementRetraction::Split>(e[1]);
        const auto &response = a.get<PreparedRefinementResponse::Result>(e[2]);
        const auto &acc = a.get<PreparedRefinementDiagram::Accumulation>(e[3]);
        const auto &contract = a.get<PreparedRefinementDiagram::LimitContract>(e[4]);
        return q(energy.total()) + "," + vector(split.private_field().values()) +
               vector(response.data().aggregate) + matrix(acc.gram()) +
               std::to_string(contract.certifies_continuum_limit());
    }
    if (name == "orbit") {
        const auto &k = a.get<PreparedNativeKrylov>(e[0]);
        const auto &o = a.get<PreparedNativeCayleyOrbit>(e[1]);
        return vector(k.polynomial()) + std::to_string(static_cast<int>(o.kind()));
    }
    throw std::runtime_error("unknown observation");
}
} // namespace c_declaration_observations
