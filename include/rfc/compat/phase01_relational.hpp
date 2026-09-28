#pragma once
#include "rfc/exact.hpp"
#include "rfc/packed_bits.hpp"
#include "rfc/compat/phase01_metric.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rfc {

struct IncidenceSpec {
    std::uint32_t lower{};
    bool exists{true};
    bool negative{false};
    Exact magnitude{1};
};

class BoundaryStore {
public:
    BoundaryStore(std::size_t lower_count, std::size_t upper_count,
                  const std::vector<std::vector<IncidenceSpec>>& relations)
        : lower_count_(lower_count), upper_count_(upper_count) {
        if (relations.size() != upper_count_) throw std::invalid_argument("BoundaryStore: relation count mismatch");
        offsets_.reserve(upper_count_ + 1u);
        offsets_.push_back(0u);
        for (const auto& rel : relations) {
            for (const auto& in : rel) {
                if (in.lower >= lower_count_) throw std::out_of_range("BoundaryStore: lower id");
                lower_.push_back(in.lower);
                magnitudes_.push_back(in.magnitude);
                existence_raw_.push_back(in.exists);
                negative_raw_.push_back(in.negative);
            }
            offsets_.push_back(lower_.size());
        }
        exists_ = PackedBits(lower_.size());
        negative_ = PackedBits(lower_.size());
        for (std::size_t i = 0; i < lower_.size(); ++i) {
            exists_.set(i, existence_raw_[i]);
            negative_.set(i, negative_raw_[i]);
        }
        existence_raw_.clear();
        negative_raw_.clear();
        existence_raw_.shrink_to_fit();
        negative_raw_.shrink_to_fit();
    }

    std::size_t lower_count() const noexcept { return lower_count_; }
    std::size_t upper_count() const noexcept { return upper_count_; }
    std::size_t incidence_count() const noexcept { return lower_.size(); }

    Exact coefficient(std::size_t incidence) const {
        if (!exists_.get(incidence)) return 0;
        return negative_.get(incidence) ? -magnitudes_.at(incidence) : magnitudes_.at(incidence);
    }

    ExactVector apply(const ExactVector& upper_field) const {
        if (upper_field.size() != upper_count_) throw std::invalid_argument("BoundaryStore::apply size mismatch");
        ExactVector lower_field(lower_count_, Exact{0});
        for (std::size_t rel = 0; rel < upper_count_; ++rel) {
            if (upper_field[rel] == 0) continue;
            for (std::size_t p = offsets_[rel]; p < offsets_[rel + 1u]; ++p) {
                if (!exists_.get(p)) continue;
                const Exact signed_mag = negative_.get(p) ? -magnitudes_[p] : magnitudes_[p];
                lower_field[lower_[p]] += signed_mag * upper_field[rel];
            }
        }
        return lower_field;
    }

    ExactVector transpose_apply(const ExactVector& lower_field) const {
        if (lower_field.size() != lower_count_) throw std::invalid_argument("BoundaryStore::transpose size mismatch");
        ExactVector upper_field(upper_count_, Exact{0});
        for (std::size_t rel = 0; rel < upper_count_; ++rel) {
            Exact acc = 0;
            for (std::size_t p = offsets_[rel]; p < offsets_[rel + 1u]; ++p) {
                if (!exists_.get(p)) continue;
                const Exact signed_mag = negative_.get(p) ? -magnitudes_[p] : magnitudes_[p];
                acc += signed_mag * lower_field[lower_[p]];
            }
            upper_field[rel] = std::move(acc);
        }
        return upper_field;
    }

    ExactVector adjoint(const ExactVector& lower_field,
                        const DiagonalMetric& lower_metric,
                        const DiagonalMetric& upper_metric) const {
        if (lower_metric.size() != lower_count_ || upper_metric.size() != upper_count_) {
            throw std::invalid_argument("BoundaryStore::adjoint metric size mismatch");
        }
        return upper_metric.inverse_apply(transpose_apply(lower_metric.apply(lower_field)));
    }

    ExactVector adjoint_identity_metric(const ExactVector& lower_field) const {
        const auto lower_metric = DiagonalMetric::identity(lower_count_);
        const auto upper_metric = DiagonalMetric::identity(upper_count_);
        return adjoint(lower_field, lower_metric, upper_metric);
    }

private:
    std::size_t lower_count_{};
    std::size_t upper_count_{};
    std::vector<std::size_t> offsets_;
    std::vector<std::uint32_t> lower_;
    std::vector<Exact> magnitudes_;
    PackedBits exists_;
    PackedBits negative_;
    std::vector<bool> existence_raw_;
    std::vector<bool> negative_raw_;
};

class RelationTable {
public:
    RelationTable(std::size_t relation_count, std::size_t type_count)
        : active_(relation_count, true), payload_(relation_count, Exact{0}), type_planes_() {
        type_planes_.reserve(type_count);
        for (std::size_t i = 0; i < type_count; ++i) type_planes_.emplace_back(relation_count, false);
    }

    std::size_t size() const noexcept { return payload_.size(); }
    void set_type(std::size_t relation, std::size_t type, bool value = true) { type_planes_.at(type).set(relation, value); }
    void set_active(std::size_t relation, bool value) { active_.set(relation, value); }
    void set_payload(std::size_t relation, Exact value) { payload_.at(relation) = std::move(value); }
    const Exact& payload(std::size_t relation) const { return payload_.at(relation); }

    PackedBits query_type(std::size_t type) const { return active_ & type_planes_.at(type); }

private:
    PackedBits active_;
    std::vector<Exact> payload_;
    std::vector<PackedBits> type_planes_;
};

} // namespace rfc
