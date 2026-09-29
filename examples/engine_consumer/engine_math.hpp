#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Example downstream ABI boundary. No RFC/GMP/Godot types escape this header.
// This is a build/integration example, not a physics model or frame-time claim.
struct BoundaryValue {
    std::uint64_t identity;
    std::string numerator, denominator;
};
class EngineMath {
public:
    EngineMath();
    ~EngineMath();
    EngineMath(EngineMath&&) noexcept;
    EngineMath& operator=(EngineMath&&) noexcept;
    EngineMath(const EngineMath&) = delete;
    EngineMath& operator=(const EngineMath&) = delete;
    void set_amplitude(std::int64_t numerator, std::uint64_t denominator);
    std::vector<BoundaryValue> boundary() const;
    std::size_t retained_relations() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
