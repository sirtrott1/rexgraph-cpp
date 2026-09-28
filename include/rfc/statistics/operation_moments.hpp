#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rfc::statistics {

struct OperationChannel {
    std::string label;
    numeric::ExactMatrix map;
};

inline numeric::ExactMatrix moment_kernel(const numeric::ExactMatrix& left,
                                          const numeric::ExactMatrix& cross_form,
                                          const numeric::ExactMatrix& right) {
    if (cross_form.rows() != left.rows() || cross_form.cols() != right.rows()) throw std::invalid_argument("operation moment cross-form mismatch");
    if (left.cols() != right.cols()) throw std::invalid_argument("operation moment source mismatch");
    return left.transpose().multiply(cross_form).multiply(right);
}

inline numeric::ExactMatrix common_target_gram(const std::vector<OperationChannel>& channels,
                                               const numeric::ExactMatrix& metric) {
    if (channels.empty()) return numeric::ExactMatrix{};
    const auto target = channels.front().map.rows();
    const auto source = channels.front().map.cols();
    if (metric.rows() != target || metric.cols() != target) throw std::invalid_argument("operation channel metric mismatch");
    numeric::ExactMatrix out(channels.size(), channels.size());
    for (std::size_t i = 0; i < channels.size(); ++i) {
        if (channels[i].map.rows() != target || channels[i].map.cols() != source) throw std::invalid_argument("operation channel dimensions differ");
        for (std::size_t j = 0; j < channels.size(); ++j) {
            const auto kernel = moment_kernel(channels[i].map, metric, channels[j].map);
            if (kernel.rows() != 1 || kernel.cols() != 1) throw std::invalid_argument("common_target_gram expects scalar-source channels");
            if (kernel.get(0,0) != 0) out.set(i,j,kernel.get(0,0));
        }
    }
    return out;
}

inline numeric::ExactMatrix channel_field_gram(const std::vector<numeric::RationalVector>& fields,
                                                const numeric::ExactMatrix& metric) {
    if (fields.empty()) return numeric::ExactMatrix{};
    const auto n = fields.front().size();
    if (metric.rows() != n || metric.cols() != n) throw std::invalid_argument("channel field metric mismatch");
    numeric::ExactMatrix out(fields.size(), fields.size());
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].size() != n) throw std::invalid_argument("channel field size mismatch");
        for (std::size_t j = 0; j < fields.size(); ++j) {
            const auto value = numeric::dot(fields[i], metric.apply(fields[j]));
            if (value != 0) out.set(i,j,value);
        }
    }
    return out;
}

} // namespace rfc::statistics
