#include "engine_math.hpp"
#include "rfc/core/builder.hpp"
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
    rfc::numeric::Rational amplitude{0};
};
EngineMath::EngineMath() : impl_(std::make_unique<Impl>()) {}
EngineMath::~EngineMath() = default;
EngineMath::EngineMath(EngineMath&&) noexcept = default;
EngineMath& EngineMath::operator=(EngineMath&&) noexcept = default;
void EngineMath::set_amplitude(std::int64_t numerator, std::uint64_t denominator) {
    if (!impl_) throw std::logic_error("moved-from EngineMath");
    if (!denominator) throw std::invalid_argument("zero exact denominator");
    rfc::numeric::Rational candidate(std::to_string(numerator) + "/" + std::to_string(denominator));
    candidate.canonicalize(); impl_->amplitude = std::move(candidate);
}
std::vector<BoundaryValue> EngineMath::boundary() const {
    if (!impl_) throw std::logic_error("moved-from EngineMath");
    const auto values = impl_->source.boundary_apply(rfc::core::Grade{1}, {impl_->amplitude});
    const auto& arena = impl_->source.grade(rfc::core::Grade{0});
    std::vector<BoundaryValue> out; out.reserve(values.size());
    for (std::size_t i = 0; i < values.size(); ++i)
        out.push_back({arena.identities[i], values[i].get_num().get_str(), values[i].get_den().get_str()});
    return out;
}
std::size_t EngineMath::retained_relations() const {
    if (!impl_) throw std::logic_error("moved-from EngineMath");
    return impl_->source.grade(rfc::core::Grade{1}).cell_count();
}
