#pragma once
#include "rfc/numeric/exact_matrix.hpp"
#include "rfc/numeric/sparse_vector.hpp"
#include "rfc/numeric/prepared_positive_semidefinite.hpp"
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
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
    using SparseVectorAction = std::function<numeric::SparseRationalVector(numeric::SparseRationalView)>;

    explicit ExactMetric(numeric::ExactMatrix form)
        : ExactMetric(std::move(form),compatibility_limits()) {}
    ExactMetric(numeric::ExactMatrix form,numeric::ExactActionLimits limits)
        : storage_(make_sparse(std::move(form),limits)) {}
    ExactMetric(numeric::ExactMatrix form,numeric::ExactActionGuard& guard)
        : storage_(SparseStorage{std::move(form),guard}) {}

    static ExactMetric identity(std::size_t n) {
        // The declared zero carrier has one positive metric, vacuously.
        return ExactMetric(IdentityStorage{n});
    }

    static ExactMetric diagonal(numeric::RationalVector diagonal) {
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
                                          Materializer materialize = {},
                                          SparseVectorAction sparse_apply = {},
                                          SparseVectorAction sparse_solve = {}) {
        if (!apply || !solve) throw std::invalid_argument("external exact metric requires apply/solve");
        return ExactMetric(ExternalStorage{n, ExactMetricKind::CertifiedExternal, std::move(apply), std::move(solve),
            std::move(materialize),std::move(sparse_apply),std::move(sparse_solve),{}});
    }

    static ExactMetric block_diagonal(std::vector<ExactMetric> blocks) {
        if (blocks.empty()) throw std::invalid_argument("block metric requires at least one block");
        auto shared=std::make_shared<std::vector<ExactMetric>>(std::move(blocks));
        std::size_t total=0;
        for(const auto& block:*shared) total=machine::checked_size_add(total,block.size());
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
        const auto sparse_action=[shared,total](numeric::SparseRationalView x,bool inverse) {
            if(x.size()!=total)throw std::invalid_argument("sparse block metric carrier mismatch");
            std::vector<numeric::SparseRationalEntry> result;
            std::size_t offset{},at{};
            for(const auto& block:*shared){
                const auto end=machine::checked_size_add(offset,block.size());
                std::vector<numeric::SparseRationalEntry> local;
                while(at<x.entries().size() && x.entries()[at].index<end){
                    const auto& entry=x.entries()[at++];
                    local.push_back({entry.index-offset,entry.value});
                }
                const numeric::SparseRationalVector input(block.size(),std::move(local));
                const auto output=inverse?block.solve(input.view()):block.apply(input.view());
                for(const auto& entry:output.entries())result.push_back({offset+entry.index,entry.value});
                offset=end;
            }
            return numeric::SparseRationalVector(total,std::move(result));
        };
        SparseVectorAction sparse_apply,sparse_solve;
        if(std::all_of(shared->begin(),shared->end(),[](const auto& b){return b.supports_sparse_apply();}))
            sparse_apply=[sparse_action](auto x){return sparse_action(x,false);};
        if(std::all_of(shared->begin(),shared->end(),[](const auto& b){return b.supports_sparse_solve();}))
            sparse_solve=[sparse_action](auto x){return sparse_action(x,true);};
        auto determinant=[shared]{numeric::Rational result{1};for(const auto& b:*shared)result*=b.determinant();return result;};
        return ExactMetric(ExternalStorage{total,ExactMetricKind::BlockDiagonal,std::move(apply),std::move(solve),
            std::move(materialize),std::move(sparse_apply),std::move(sparse_solve),std::move(determinant)});
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

    // Read-only retained declarations for persistence. These never materialize
    // an identity, invoke an external callback, or expose prepared factors.
    [[nodiscard]] const numeric::RationalVector* stored_diagonal() const & noexcept {
        const auto* d = std::get_if<DiagonalStorage>(&storage_);
        return d ? &d->diagonal : nullptr;
    }
    const numeric::RationalVector* stored_diagonal() const && = delete;
    [[nodiscard]] const numeric::ExactMatrix* stored_sparse_form() const & noexcept {
        const auto* s = std::get_if<SparseStorage>(&storage_);
        return s ? &s->certificate.original() : nullptr;
    }
    const numeric::ExactMatrix* stored_sparse_form() const && = delete;

    [[nodiscard]] bool is_diagonal() const noexcept {
        return std::holds_alternative<IdentityStorage>(storage_) || std::holds_alternative<DiagonalStorage>(storage_);
    }

    [[nodiscard]] bool supports_sparse_apply() const noexcept {
        const auto* external=std::get_if<ExternalStorage>(&storage_);
        return !external || static_cast<bool>(external->sparse_apply);
    }
    [[nodiscard]] bool supports_sparse_solve() const noexcept {
        const auto* external=std::get_if<ExternalStorage>(&storage_);
        return !external || static_cast<bool>(external->sparse_solve);
    }

    [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& x) const {
        if (x.size() != size()) throw std::invalid_argument("metric apply size mismatch");
        return std::visit([&](const auto& value) { return value.apply(x); }, storage_);
    }

    [[nodiscard]] numeric::RationalVector solve(const numeric::RationalVector& x) const {
        if (x.size() != size()) throw std::invalid_argument("metric solve size mismatch");
        return std::visit([&](const auto& value) { return value.solve(x); }, storage_);
    }

    // Unit and diagonal metrics act on contiguous sample rows. Exact in place
    // application is permitted; partially overlapping buffers are rejected.
    void apply_batch_into(std::span<const numeric::Rational> input,
                          std::span<numeric::Rational> output, std::size_t samples) const {
        diagonal_batch_into(input, output, samples, false);
    }
    void solve_batch_into(std::span<const numeric::Rational> input,
                          std::span<numeric::Rational> output, std::size_t samples) const {
        diagonal_batch_into(input, output, samples, true);
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
        if(const auto* sparse=std::get_if<SparseStorage>(&storage_))return sparse->apply(x);
        const auto& external=std::get<ExternalStorage>(storage_);
        if(!external.sparse_apply)throw std::logic_error("external metric has no declared sparse apply action");
        auto result=external.sparse_apply(x);
        if(result.size()!=size())throw std::logic_error("external sparse metric output carrier mismatch");
        return result;
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
        if(const auto* sparse=std::get_if<SparseStorage>(&storage_))return sparse->solve(x);
        const auto& external=std::get<ExternalStorage>(storage_);
        if(!external.sparse_solve)throw std::logic_error("external metric has no declared sparse solve action");
        auto result=external.sparse_solve(x);
        if(result.size()!=size())throw std::logic_error("external sparse metric solve carrier mismatch");
        return result;
    }

    [[nodiscard]] numeric::Rational inner_sparse(numeric::SparseRationalView x,numeric::SparseRationalView y) const {
        if(x.size()!=size() || y.size()!=size())throw std::invalid_argument("sparse metric pairing carrier mismatch");
        const auto measured=apply(y);numeric::Rational result{0};std::size_t i{},j{};
        while(i<x.entries().size() && j<measured.entries().size()){
            const auto& a=x.entries()[i];const auto& b=measured.entries()[j];
            if(a.index<b.index)++i;else if(b.index<a.index)++j;
            else {result+=a.value*b.value;++i;++j;}
        }
        result.canonicalize();return result;
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
        if (const auto* sparse = std::get_if<SparseStorage>(&storage_)) return sparse->determinant();
        if(const auto& external=std::get<ExternalStorage>(storage_);external.determinant)return external.determinant();
        return materialize().determinant();
    }

    // Explicit coordinate realization. Production apply/solve paths do not call
    // this method. `form()` remains as a compatibility spelling for older code.
    [[nodiscard]] numeric::ExactMatrix materialize() const {
        if (const auto* identity = std::get_if<IdentityStorage>(&storage_)) return numeric::ExactMatrix::identity(identity->n);
        if (const auto* diagonal = std::get_if<DiagonalStorage>(&storage_)) return numeric::ExactMatrix::diagonal(diagonal->diagonal);
        if (const auto* sparse = std::get_if<SparseStorage>(&storage_)) return sparse->certificate.original();
        const auto& external = std::get<ExternalStorage>(storage_);
        if (!external.materialize) throw std::logic_error("external exact metric has no coordinate materializer");
        auto out = external.materialize();
        validate_square_symmetric(out);
        if (out.rows() != external.n) throw std::logic_error("external exact metric materializer size mismatch");
        return out;
    }

    [[nodiscard]] const numeric::ExactMatrix& form() const {
        if (const auto* sparse = std::get_if<SparseStorage>(&storage_)) return sparse->certificate.original();
        materialized_cache_ = materialize();
        return *materialized_cache_;
    }

private:
    void diagonal_batch_into(std::span<const numeric::Rational> input,
                             std::span<numeric::Rational> output, std::size_t samples,
                             bool inverse) const {
        if (!is_diagonal()) throw std::logic_error("metric batch requires a unit or diagonal metric");
        if (!samples || input.size() != machine::checked_size_mul(size(), samples) || output.size() != input.size())
            throw std::invalid_argument("metric batch buffer shape mismatch");
        const std::less<const numeric::Rational*> before;
        if (input.data() != output.data() && before(input.data(), output.data() + output.size()) &&
            before(output.data(), input.data() + input.size()))
            throw std::invalid_argument("metric batch buffers partially overlap");
        const auto* diagonal = std::get_if<DiagonalStorage>(&storage_);
        for (std::size_t cell = 0; cell < size(); ++cell)
            for (std::size_t s = 0; s < samples; ++s) {
                const auto index = cell * samples + s;
                if (!diagonal) output[index] = input[index];
                else if (inverse) output[index] = input[index] / diagonal->diagonal[cell];
                else output[index] = input[index] * diagonal->diagonal[cell];
            }
    }
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
        using Q=numeric::Rational;
        using Coordinates=std::map<std::size_t,Q>;
        using Index=std::map<std::size_t,std::vector<std::pair<std::size_t,Q>>>;
        numeric::PreparedPositiveSemidefinite certificate;
        Index columns,lower_columns,lower_rows;
        SparseStorage(numeric::ExactMatrix form,numeric::ExactActionGuard& guard)
            :certificate(std::move(form),guard) {
            if(certificate.rank()!=certificate.size())throw std::invalid_argument("metric must be positive definite");
            const auto entries=machine::checked_size_add(
                machine::checked_size_mul(certificate.original().nonzeros(),2),
                machine::checked_size_add(machine::checked_size_mul(certificate.unit_lower_entries().nonzeros(),3),certificate.rank()));
            guard.entries(entries);
            for(const auto& [key,q]:certificate.original().entries()){guard.step();columns[key.second].emplace_back(key.first,q);}
            for(const auto& [key,q]:certificate.unit_lower_entries().entries()){
                guard.step();
                lower_columns[key.second].emplace_back(key.first,q);lower_rows[key.first].emplace_back(key.second,q);
            }
        }
        [[nodiscard]] std::size_t size() const noexcept { return certificate.size(); }
        [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& x) const { return certificate.original().apply(x); }
        [[nodiscard]] numeric::RationalVector solve(const numeric::RationalVector& x) const {
            const auto input=numeric::canonical_vector(x);auto out=input;
            const auto& lower=certificate.unit_lower_entries().entries();
            for(const auto& [key,q]:lower)out[key.first]-=q*out[key.second];
            for(const auto& [i,pivot]:certificate.diagonal_entries())out[i]/=pivot;
            for(auto it=lower.rbegin();it!=lower.rend();++it)out[it->first.second]-=it->second*out[it->first.first];
            if(certificate.original().apply(out)!=input)throw std::runtime_error("metric LDL solve residual is nonzero");
            return out;
        }
        static void add(Coordinates& values,std::size_t i,Q amount){
            if(amount==0)return;
            const auto it=values.find(i);
            if(it!=values.end())amount+=it->second;
            amount.canonicalize();
            if(amount==0){if(it!=values.end())values.erase(it);}
            else values[i]=std::move(amount);
        }
        numeric::SparseRationalVector vector(Coordinates values) const {
            std::vector<numeric::SparseRationalEntry> out;out.reserve(values.size());
            for(auto& [i,q]:values)out.push_back({i,std::move(q)});
            return numeric::SparseRationalVector(size(),std::move(out));
        }
        [[nodiscard]] numeric::SparseRationalVector apply(numeric::SparseRationalView x) const {
            Coordinates result;
            for(const auto& entry:x.entries()){
                const auto column=columns.find(entry.index);
                if(column!=columns.end())for(const auto& [i,q]:column->second)add(result,i,Q{q*entry.value});
            }
            return vector(std::move(result));
        }
        [[nodiscard]] numeric::SparseRationalVector solve(numeric::SparseRationalView x) const {
            const numeric::SparseRationalVector input(size(),std::vector<numeric::SparseRationalEntry>(x.entries().begin(),x.entries().end()));
            Coordinates result;for(const auto& entry:input.entries())result.emplace(entry.index,entry.value);
            // A newly reached coordinate joins the ordered frontier. Absent
            // factor columns and zero solution coordinates are never expanded.
            for(auto at=result.begin();at!=result.end();++at){
                const auto [i,value]=*at;const auto column=lower_columns.find(i);
                if(column!=lower_columns.end())for(const auto& [j,q]:column->second)add(result,j,Q{-q*value});
            }
            for(auto& [i,q]:result)q/=certificate.diagonal_entries().at(i);
            for(auto at=result.rbegin();at!=result.rend();++at){
                const auto [i,value]=*at;const auto row=lower_rows.find(i);
                if(row!=lower_rows.end())for(const auto& [j,q]:row->second)add(result,j,Q{-q*value});
            }
            auto out=vector(std::move(result));
            if(apply(out.view()).entries()!=input.entries())throw std::runtime_error("sparse metric LDL solve residual is nonzero");
            return out;
        }
        [[nodiscard]] Q determinant() const {
            Q result{1};for(const auto& [i,pivot]:certificate.diagonal_entries()){(void)i;result*=pivot;}return result;
        }
    };

    struct ExternalStorage {
        std::size_t n{};
        ExactMetricKind kind{ExactMetricKind::CertifiedExternal};
        VectorAction apply_action;
        VectorAction solve_action;
        Materializer materialize;
        SparseVectorAction sparse_apply,sparse_solve;
        std::function<numeric::Rational()> determinant;
        [[nodiscard]] std::size_t size() const noexcept { return n; }
        [[nodiscard]] numeric::RationalVector apply(const numeric::RationalVector& x) const {
            auto out=apply_action(x);if(out.size()!=n)throw std::logic_error("external metric output carrier mismatch");return out;
        }
        [[nodiscard]] numeric::RationalVector solve(const numeric::RationalVector& x) const {
            auto out=solve_action(x);if(out.size()!=n)throw std::logic_error("external metric solve carrier mismatch");return out;
        }
    };

    using Storage = std::variant<IdentityStorage, DiagonalStorage, SparseStorage, ExternalStorage>;

    explicit ExactMetric(IdentityStorage storage) : storage_(std::move(storage)) {}
    explicit ExactMetric(DiagonalStorage storage) : storage_(std::move(storage)) {}
    explicit ExactMetric(ExternalStorage storage) : storage_(std::move(storage)) {}

    static void validate_square_symmetric(const numeric::ExactMatrix& form) {
        if (form.rows() != form.cols()) throw std::invalid_argument("metric must be square form");
        for (const auto& [key, value] : form.entries()) {
            if (value != form.get(key.second, key.first)) throw std::invalid_argument("metric must be symmetric");
        }
    }

    // Preserve the original unrestricted constructor contract. Callers with an
    // execution policy can pass ExactActionLimits to the explicit overload.
    static SparseStorage make_sparse(numeric::ExactMatrix form,numeric::ExactActionLimits limits) {
        numeric::ExactActionGuard guard(limits);return SparseStorage(std::move(form),guard);
    }
    static numeric::ExactActionLimits compatibility_limits() {
        const auto maximum=std::numeric_limits<std::size_t>::max();
        return {maximum,maximum,maximum,maximum};
    }

    Storage storage_;
    mutable std::optional<numeric::ExactMatrix> materialized_cache_;
};

} // namespace rfc::field
