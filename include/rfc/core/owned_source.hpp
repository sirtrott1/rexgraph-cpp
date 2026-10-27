#pragma once

#include "rfc/core/relational_complex.hpp"
#include <memory>

namespace rfc::core {

// An immutable snapshot of retained source data, not a publication or a proof.
// Copies of this handle share identity. A new snapshot always has a new owner,
// even if every retained cell, occurrence and coefficient has equal content.
// There is no constructor adopting a possibly mutable external shared_ptr.
class OwnedRelationalSource {
public:
    explicit OwnedRelationalSource(const RelationalComplex& source)
        : source_(std::make_shared<const RelationalComplex>(source)) {}
    OwnedRelationalSource(const OwnedRelationalSource&) = default;
    OwnedRelationalSource& operator=(const OwnedRelationalSource&) = default;

    [[nodiscard]] const RelationalComplex& source() const & noexcept { return *source_; }
    const RelationalComplex& source() const && = delete;
    [[nodiscard]] std::shared_ptr<const RelationalComplex> shared_source() const noexcept {
        return source_;
    }
    [[nodiscard]] bool same_owner(const OwnedRelationalSource& other) const noexcept {
        return source_ == other.source_;
    }
private:
    std::shared_ptr<const RelationalComplex> source_;
};

} // namespace rfc::core
