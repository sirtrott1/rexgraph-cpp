#include "engine_math.hpp"
#include "rfc/core/builder.hpp"
#include "rfc/database/native_execution.hpp"
#include <stdexcept>

namespace {
rfc::core::RelationalComplex make_source() {
    using namespace rfc;
    core::RelationSignature signature;
    const auto type = signature.add_type("oriented-pair", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature);
    builder.add_grade_one(core::RelationKey{100}, type, {
        {10, true, true, numeric::Rational{1}, {}, core::OccurrenceKey{1000}},
        {11, true, false, numeric::Rational{1}, {}, core::OccurrenceKey{1001}}});
    return builder.freeze();
}
}
struct EngineMath::Impl {
    rfc::core::RelationalComplex source = make_source();
    struct PreparedState {
        rfc::database::PreparedNativeField operation;
        rfc::database::NativeFieldWorkspace workspace;
        explicit PreparedState(rfc::database::PreparedNativeField value)
            : operation(std::move(value)), workspace(rfc::database::make_workspace(operation)) {}
    };
    std::unique_ptr<PreparedState> prepared;
    Impl() { prepared=std::make_unique<PreparedState>(prepare(rfc::numeric::Rational{0})); }
    rfc::database::PreparedNativeField prepare(const rfc::numeric::Rational& amplitude) const {
        using namespace rfc;
        using namespace rfc::storage;
        TemporalEndpoint endpoint(source);
        // This adapter declares one shared scalar amplitude coordinate. It is
        // a host interpretation, not an inferred identity between equal sizes.
        for(const auto key:{10u,11u})endpoint.carriers.push_back({key,TemporalCarrierKind::CellStalk,core::Grade{0},StableCellKey{core::Grade{0},key},900,numeric::IndexDomain::linear(1)});
        endpoint.carriers.push_back({100,TemporalCarrierKind::CellStalk,core::Grade{1},StableCellKey{core::Grade{1},100},900,numeric::IndexDomain::linear(1)});
        TemporalTensor field;field.key=200;field.role=TemporalTensorRole::Field;field.output=100;
        field.owner.kind=TemporalOwnerKind::Cell;field.owner.cell={core::Grade{1},100};
        field.interpretation="engine adapter exact scalar amplitude in common coordinate 900";
        field.tensor={numeric::IndexDomain::linear(1),numeric::StructuralSupport::full(1),numeric::SparseRationalVector(1,{{0,amplitude}})};
        endpoint.tensors.push_back(field);
        NativeAccessionDirectory directory(endpoint,0x454e47494e45ULL);
        NativeAccessionRequest input;input.kind=NativeAccessionKind::Field;input.subject=200;
        auto pin=directory.pin(input);
        NativeFieldRequest request;request.stalk_carriers={10,11,100};request.fields={pin};
        return database::prepare_field_action(pin,request,NativeFieldRealization::constant_fiber(900));
    }
};
EngineMath::EngineMath() : impl_(std::make_unique<Impl>()) {}
EngineMath::~EngineMath() = default;
EngineMath::EngineMath(EngineMath&&) noexcept = default;
EngineMath& EngineMath::operator=(EngineMath&&) noexcept = default;
void EngineMath::set_amplitude(std::int64_t numerator, std::uint64_t denominator) {
    if (!impl_) throw std::logic_error("moved-from EngineMath");
    if (!denominator) throw std::invalid_argument("zero exact denominator");
    rfc::numeric::Rational candidate(std::to_string(numerator) + "/" + std::to_string(denominator));
    candidate.canonicalize();
    auto prepared=std::make_unique<Impl::PreparedState>(impl_->prepare(candidate));
    impl_->prepared=std::move(prepared);
}
std::vector<BoundaryValue> EngineMath::boundary() const {
    if (!impl_) throw std::logic_error("moved-from EngineMath");
    const auto result=rfc::database::execute_prepared(impl_->prepared->operation,impl_->prepared->workspace);
    std::vector<BoundaryValue> out; out.reserve(result.carriers().size());
    for (const auto& carrier : result.carriers()) {
        const auto& value=result.state(carrier.cell,0);
        out.push_back({carrier.cell.key,value.get_num().get_str(),value.get_den().get_str()});
    }
    return out;
}
std::size_t EngineMath::retained_relations() const {
    if (!impl_) throw std::logic_error("moved-from EngineMath");
    return impl_->source.grade(rfc::core::Grade{1}).cell_count();
}
