#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace rfc::field {

enum class ExactMetricKind : unsigned char {
    Identity,
    Diagonal,
    SparseMatrix,
    BlockDiagonal,
    CertifiedExternal
};

// Exact positive metric represented by actions first. Identity and diagonal
// metrics never need a matrix allocation for apply/solve/inner. General sparse
// metrics retain an ExactMatrix reference implementation. Certified external
// metrics allow an exact backend to supply apply/solve without exposing storage.
class ExactMetric {
public:
    using VectorAction = std::function<numeric::RationalVector(const numeric::RationalVector&)>;
    using Materializer = std::function<numeric::ExactMatrix()>;

    explicit ExactMetric(numeric::ExactMatrix form)
        : storage_(SparseStorage{std::move(form)}) {
        const auto& matrix = std::get<SparseStorage>(storage_).form;
        validate_square_symmetric(matrix);
        certify_positive_definite_sparse(matrix);
    }

    static ExactMetric identity(std::size_t n) {
        if (n == 0) throw std::invalid_argument("metric must be nonempty");
        return ExactMetric(IdentityStorage{n});
    }

    static ExactMetric diagonal(numeric::RationalVector diagonal) {
        if (diagonal.empty()) throw std::invalid_argument("metric must be nonempty");
        for (auto& value : diagonal) {
            value.canonicalize();
            if (value <= 0) throw std::invalid_argument("metric diagonal must be positive");
        }
        return ExactMetric(DiagonalStorage{std::move(diagonal)});
    }

    // The caller certifies that these actions represent one symmetric positive
    // definite exact metric. A materializer is optional and is used only by
    // explicit diagnostic/reference operations that request the coordinate form.
    static ExactMetric certified_external(std::size_t n,
                                          VectorAction apply,
                                          VectorAction solve,
                                          Materializer materialize = {}) {
        if (n == 0 || !apply || !solve) throw std::invalid_argument("external exact metric requires size/apply/solve");
        return ExactMetric(ExternalStorage{n, ExactMetricKind::CertifiedExternal, std::move(apply), std::move(solve), std::move(materialize)});
    }

    static ExactMetric block_diagonal(std::vector<ExactMetric> blocks) {
        if (blocks.empty()) throw std::invalid_argument("block metric requires at least one block");
        auto shared=std::make_shared<std::vector<ExactMetric>>(std::move(blocks));
        std::size_t total=0;
        for(const auto& block:*shared) total+=block.size();
        auto apply=[shared,total](const numeric::RationalVector& x){
            if(x.size()!=total) throw std::invalid_argument("block metric apply size mismatch");
            numeric::RationalVector out(total,numeric::Rational{0});
            std::size_t offset=0;
            for(const auto& block:*shared){
                numeric::RationalVector local(block.size());
                for(std::size_t i=0;i<block.size();++i)local[i]=x[offset+i];
                const auto y=block.apply(local);
                for(std::size_t i=0;i<block.size();++i)out[offset+i]=y[i];
                offset+=block.size();
            }
            return out;
        };
        auto solve=[shared,total](const numeric::RationalVector& x){
            if(x.size()!=total) throw std::invalid_argument("block metric solve size mismatch");
            numeric::RationalVector out(total,numeric::Rational{0});
            std::size_t offset=0;
            for(const auto& block:*shared){
                numeric::RationalVector local(block.size());
                for(std::size_t i=0;i<block.size();++i)local[i]=x[offset+i];
                const auto y=block.solve(local);
                for(std::size_t i=0;i<block.size();++i)out[offset+i]=y[i];
                offset+=block.size();
            }
            return out;
        };
        auto materialize=[shared,total](){
            numeric::ExactMatrix out(total,total);
            std::size_t offset=0;
            for(const auto& block:*shared){
                const auto matrix=block.materialize();
                for(const auto& [key,value]:matrix.entries())out.set(offset+key.first,offset+key.second,value);
                offset+=block.size();
            }
            return out;
        };
        return ExactMetric(ExternalStorage{total,ExactMetricKind::BlockDiagonal,std::move(apply),std::move(solve),std::move(materialize)});
    }

    [[nodiscard]] ExactMetricKind kind() const noexcept {
        if (std::holds_alternative<IdentityStorage>(storage_)) return ExactMetricKind::Identity;
        if (std::holds_alternative<DiagonalStorage>(storage_)) return ExactMetricKind::Diagonal;
        if (std::holds_alternative<SparseStorage>(storage_)) return ExactMetricKind::SparseMatrix;
        return std::get<ExternalStorage>(storage_).kind;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return std::visit([](const auto& value) { return value.size(); }, storage_);
    }

    [[nodiscard]] bool is_diagonal() const noexcept {
        return std::holds_alternative<IdentityStorage>(storage_) || std::holds_alternative<DiagonalStorage>(storage_);
    }

    [[nodiscard]] bool supports_sparse_solve() const noexcept { return is_diagonal(); }

    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& x) const {
        if (x.size() != size()) throw std::invalid_argument("metric apply size mismatch");
        return std::visit([&](const auto& value) { return value.apply(x); }, storage_);
    }

    [[nodiscard]] numeric::RationalVector solve(const numeric::RationalVector& x) const {
        if (x.size() != size()) throw std::invalid_argument("metric solve size mismatch");
        return std::visit([&](const auto& value) { return value.solve(x); }, storage_);
    }

    [[nodiscard]] numeric::SparseRationalVector apply(numeric::SparseRationalView x) const {
        if (x.size() != size()) throw std::invalid_argument("sparse metric apply size mismatch");
        if (const auto* identity = std::get_if<IdentityStorage>(&storage_)) {
            (void)identity;
            return numeric::SparseRationalVector(size(), std::vector<numeric::SparseRationalEntry>(x.entries().begin(), x.entries().end()));
        }
        if (const auto* diagonal = std::get_if<DiagonalStorage>(&storage_)) {
            std::vector<numeric::SparseRationalEntry> out;
            out.reserve(x.nonzeros());
            for (const auto& entry : x.entries()) out.push_back({entry.index, entry.value * diagonal->diagonal[entry.index]});
            return numeric::SparseRationalVector(size(), std::move(out));
        }
        return numeric::SparseRationalVector(size(), sparse_from_dense(apply(dense_from_sparse(x))));
    }

    [[nodiscard]] numeric::SparseRationalVector solve(numeric::SparseRationalView x) const {
        if (x.size() != size()) throw std::invalid_argument("sparse metric solve size mismatch");
        if (const auto* identity = std::get_if<IdentityStorage>(&storage_)) {
            (void)identity;
            return numeric::SparseRationalVector(size(), std::vector<numeric::SparseRationalEntry>(x.entries().begin(), x.entries().end()));
        }
        if (const auto* diagonal = std::get_if<DiagonalStorage>(&storage_)) {
            std::vector<numeric::SparseRationalEntry> out;
            out.reserve(x.nonzeros());
            for (const auto& entry : x.entries()) out.push_back({entry.index, entry.value / diagonal->diagonal[entry.index]});
            return numeric::SparseRationalVector(size(), std::move(out));
        }
        return numeric::SparseRationalVector(size(), sparse_from_dense(solve(dense_from_sparse(x))));
    }

    [[nodiscard]] numeric::Rational inner(const numeric::RationalVector& x,
                                          const numeric::RationalVector& y) const {
        if (x.size() != size() || y.size() != size()) throw std::invalid_argument("metric inner size mismatch");
        if (const auto* identity = std::get_if<IdentityStorage>(&storage_)) {
            (void)identity;
            return numeric::dot(x, y);
        }
        if (const auto* diagonal = std::get_if<DiagonalStorage>(&storage_)) {
            numeric::Rational result = 0;
            for (std::size_t i = 0; i < size(); ++i) result += x[i] * diagonal->diagonal[i] * y[i];
            return result;
        }
        return numeric::dot(x, apply(y));
    }

    [[nodiscard]] numeric::Rational determinant() const {
        if (std::holds_alternative<IdentityStorage>(storage_)) return numeric::Rational{1};
        if (const auto* diagonal = std::get_if<DiagonalStorage>(&storage_)) {
            numeric::Rational result = 1;
            for (const auto& value : diagonal->diagonal) result *= value;
            result.canonicalize();
            return result;
        }
        if (const auto* sparse = std::get_if<SparseStorage>(&storage_)) return sparse->form.determinant();
        return materialize().determinant();
    }

    // Explicit coordinate realization. Production apply/solve paths do not call
    // this method. `form()` remains as a compatibility spelling for older code.
    [[nodiscard]] numeric::ExactMatrix materialize() const {
        if (const auto* identity = std::get_if<IdentityStorage>(&storage_)) return numeric::ExactMatrix::identity(identity->n);
        if (const auto* diagonal = std::get_if<DiagonalStorage>(&storage_)) return numeric::ExactMatrix::diagonal(diagonal->diagonal);
        if (const auto* sparse = std::get_if<SparseStorage>(&storage_)) return sparse->form;
        const auto& external = std::get<ExternalStorage>(storage_);
        if (!external.materialize) throw std::logic_error("external exact metric has no coordinate materializer");
        auto out = external.materialize();
        validate_square_symmetric(out);
        if (out.rows() != external.n) throw std::logic_error("external exact metric materializer size mismatch");
        return out;
    }

    [[nodiscard]] const numeric::ExactMatrix& form() const {
        if (const auto* sparse = std::get_if<SparseStorage>(&storage_)) return sparse->form;
        materialized_cache_ = materialize();
        return *materialized_cache_;
    }

private:
    struct IdentityStorage {
        std::size_t n{};
        [[nodiscard]] std::size_t size() const noexcept { return n; }
        [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& x) const { return x; }
        [[nodiscard]] numeric::RationalVector solve(const numeric::RationalVector& x) const { return x; }
    };

    struct DiagonalStorage {
        numeric::RationalVector diagonal;
        [[nodiscard]] std::size_t size() const noexcept { return diagonal.size(); }
        [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& x) const {
            numeric::RationalVector out(x.size());
            for (std::size_t i = 0; i < x.size(); ++i) out[i] = diagonal[i] * x[i];
            return out;
        }
        [[nodiscard]] numeric::RationalVector solve(const numeric::RationalVector& x) const {
            numeric::RationalVector out(x.size());
            for (std::size_t i = 0; i < x.size(); ++i) out[i] = x[i] / diagonal[i];
            return out;
        }
    };

    struct SparseStorage {
        numeric::ExactMatrix form;
        [[nodiscard]] std::size_t size() const noexcept { return form.rows(); }
        [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& x) const { return form.apply(x); }
        [[nodiscard]] numeric::RationalVector solve(const numeric::RationalVector& x) const { return form.solve(x); }
    };

    struct ExternalStorage {
        std::size_t n{};
        ExactMetricKind kind{ExactMetricKind::CertifiedExternal};
        VectorAction apply_action;
        VectorAction solve_action;
        Materializer materialize;
        [[nodiscard]] std::size_t size() const noexcept { return n; }
        [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& x) const { return apply_action(x); }
        [[nodiscard]] numeric::RationalVector solve(const numeric::RationalVector& x) const { return solve_action(x); }
    };

    using Storage = std::variant<IdentityStorage, DiagonalStorage, SparseStorage, ExternalStorage>;

    explicit ExactMetric(IdentityStorage storage) : storage_(std::move(storage)) {}
    explicit ExactMetric(DiagonalStorage storage) : storage_(std::move(storage)) {}
    explicit ExactMetric(ExternalStorage storage) : storage_(std::move(storage)) {}

    static void validate_square_symmetric(const numeric::ExactMatrix& form) {
        if (form.rows() == 0 || form.rows() != form.cols()) throw std::invalid_argument("metric must be nonempty square form");
        for (const auto& [key, value] : form.entries()) {
            if (value != form.get(key.second, key.first)) throw std::invalid_argument("metric must be symmetric");
        }
    }

    static void certify_positive_definite_sparse(const numeric::ExactMatrix& form) {
        // Exact sparse LDL^T without square roots. Only nonzero L entries are
        // retained, so certification does not allocate an n-by-n dense work array.
        const std::size_t n = form.rows();
        std::vector<std::map<std::size_t, numeric::Rational>> L(n);
        numeric::RationalVector D(n, numeric::Rational{0});
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < i; ++j) {
                numeric::Rational sum = form.get(i, j);
                for (const auto& [k, lik] : L[i]) {
                    if (k >= j) break;
                    const auto jt = L[j].find(k);
                    if (jt != L[j].end()) sum -= lik * D[k] * jt->second;
                }
                if (D[j] == 0) throw std::invalid_argument("metric LDL pivot vanished");
                numeric::Rational lij = sum / D[j];
                lij.canonicalize();
                if (lij != 0) L[i][j] = std::move(lij);
            }
            numeric::Rational pivot = form.get(i, i);
            for (const auto& [k, lik] : L[i]) pivot -= lik * lik * D[k];
            pivot.canonicalize();
            if (pivot <= 0) throw std::invalid_argument("metric must be positive definite");
            D[i] = std::move(pivot);
        }
    }

    static numeric::RationalVector dense_from_sparse(numeric::SparseRationalView x) {
        numeric::RationalVector out(x.size(), numeric::Rational{0});
        for (const auto& entry : x.entries()) out[entry.index] = entry.value;
        return out;
    }

    static std::vector<numeric::SparseRationalEntry> sparse_from_dense(const numeric::RationalVector& x) {
        std::vector<numeric::SparseRationalEntry> out;
        for (std::size_t i = 0; i < x.size(); ++i) if (x[i] != 0) out.push_back({i, x[i]});
        return out;
    }

    Storage storage_;
    mutable std::optional<numeric::ExactMatrix> materialized_cache_;
};

} // namespace rfc::field
