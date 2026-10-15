#include "rfc/core/builder.hpp"
#include "rfc/inference/native_field_execution.hpp"
#include <iostream>

int main() {
    try {
        using namespace rfc;
        core::RelationSignature signature;
        const auto edge = signature.add_type("square-edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
        core::RelationalComplexBuilder builder(signature);
        for (std::uint64_t i = 0; i < 4; ++i)
            builder.add_grade_one(core::RelationKey{100 + i}, edge,
                {{i, true, true, numeric::Rational{1}}, {(i + 1) % 4, true, false, numeric::Rational{1}}});
        storage::TemporalEndpoint endpoint(builder.freeze());
        for (std::uint16_t k = 0; k < 2; ++k)
            endpoint.carriers.push_back({10u + k, storage::TemporalCarrierKind::SourceGrade, core::Grade{k}, {}, 0,
                numeric::IndexDomain::linear(4)});
        for (std::uint64_t i = 0; i < 2; ++i) {
            storage::TemporalTensor field; field.key = 1000 + i;
            field.role = storage::TemporalTensorRole::Field; field.output = 11;
            field.tensor = {numeric::IndexDomain::linear(4), numeric::StructuralSupport::full(4),
                numeric::SparseRationalVector(4, {{static_cast<std::size_t>(i), numeric::Rational{1}}})};
            endpoint.tensors.push_back(std::move(field));
        }
        storage::NativeAccessionDirectory directory(endpoint, 500);
        const auto pin = [&](std::uint64_t key) {
            storage::NativeAccessionRequest request; request.kind = storage::NativeAccessionKind::Field; request.subject = key;
            return directory.pin(request);
        };
        inference::NativeExactTowerRequest request; request.grade_carriers = {10, 11};
        request.profile = field::PreparedExactProfile::GlobalActions;
        const auto tower = inference::PreparedNativeExactTower::prepare(pin(1000), request);
        for (const auto kind : {field::OperatorKind::Green, field::OperatorKind::HarmonicProjector,
                               field::OperatorKind::HarmonicInverse, field::OperatorKind::GradientPotential}) {
            const auto action = tower.prepare_action({kind, core::Grade{1}, kind == field::OperatorKind::Green ? 1 : 0});
            const auto label = kind == field::OperatorKind::Green ? "Green" :
                kind == field::OperatorKind::HarmonicProjector ? "Harmonic projector" :
                kind == field::OperatorKind::HarmonicInverse ? "Harmonic inverse" : "Gradient potential";
            std::cout << label
                      << " preparation_steps=" << action.preparation_steps() << " retained_entries=" << action.retained_entries() << '\n';
            for (std::uint64_t key = 1000; key < 1002; ++key) {
                const auto result = action.execute(pin(key));
                std::cout << "field=" << key << " output_carrier=" << result.provenance().output_carrier << " values=";
                for (const auto& value : result.values()) std::cout << ' ' << value;
                std::cout << '\n';
            }
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
