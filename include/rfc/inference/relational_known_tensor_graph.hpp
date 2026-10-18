#pragma once

#include "rfc/inference/relational_literal_input.hpp"
#include "rfc/inference/relational_field_extension.hpp"

namespace rfc::inference {
struct RelationalKnownTensorGraphBlock {
    core::CellId cell;
    std::uint64_t source_basis{}, lifted_basis{};
    std::vector<std::uint64_t> left, right, products;
};
struct RelationalKnownTensorGraphDeclaration {
    std::uint64_t key{}, revision{};
    std::vector<RelationalKnownTensorGraphBlock> blocks;
};
class RelationalKnownTensorGraphInput;
namespace known_tensor_graph_detail {
using namespace field_interface_detail;
inline void policy(numeric::ExactActionLimits a, numeric::ExactActionLimits b) {
    require(a.dimension == b.dimension && a.entries == b.entries && a.steps == b.steps && a.integer_bits == b.integer_bits,
        "known tensor graph requires its original action policy");
}
inline std::vector<std::size_t> axes(const StalkBasisDeclaration& s, const std::vector<std::uint64_t>& names, Guard& guard) {
    require(!names.empty(), "known tensor graph requires nonempty ordered operands");
    std::vector<std::size_t> out; std::set<std::uint64_t> seen;
    for (const auto name : names) {
        guard.step(); const auto found = std::find(s.directions.begin(), s.directions.end(), name);
        require(found != s.directions.end() && seen.insert(name).second, "known tensor operand direction absent or repeated");
        out.push_back(static_cast<std::size_t>(found - s.directions.begin()));
    }
    return out;
}
inline Matrix principal(const Matrix& metric, const std::vector<std::size_t>& axes, Guard& guard) {
    std::map<std::size_t,std::size_t> positions;
    for (std::size_t i = 0; i < axes.size(); ++i) positions.emplace(axes[i], i);
    Matrix out(axes.size(), axes.size());
    for (const auto& [ij,q] : metric.entries()) {
        guard.step(); const auto i = positions.find(ij.first), j = positions.find(ij.second);
        if (i != positions.end() && j != positions.end()) out.set(i->second,j->second,q);
    }
    guard.matrix(out); return out;
}
inline bool same_stalk(const StalkBasisDeclaration& a, const StalkBasisDeclaration& b) {
    return a.cell == b.cell && a.basis == b.basis && a.directions == b.directions && a.metric == b.metric;
}
}

// A known operation on the complete original source-input field. Its image is
// a product graph, not a linear accession or a product of unknown parameters.
class OwnedRelationalKnownTensorGraph {
    using Guard = numeric::ExactActionGuard;
    using Matrix = numeric::ExactMatrix;
    using Tensor = numeric::SparseRationalTensor;
    struct Product { core::CellId cell; Tensor field; };
    struct State {
        RelationalAccessionInput parent;
        std::optional<OwnedRelationalLiteralInput> literal;
        RelationalKnownTensorGraphDeclaration declaration;
        std::vector<StalkBasisDeclaration> stalks;
        Tensor lifted{numeric::IndexDomain({0}),numeric::StructuralSupport::full(0),numeric::SparseRationalVector(0)};
        std::vector<Product> products;
        Matrix metric, original_metric, projection, section;
        numeric::ExactActionLimits limits;
        std::size_t retained{};
        State(RelationalAccessionInput input, std::optional<OwnedRelationalLiteralInput> recipe,
            RelationalKnownTensorGraphDeclaration d, Guard& guard)
            : parent(std::move(input)), literal(std::move(recipe)), declaration(std::move(d)), limits(guard.limits()) {
            using namespace known_tensor_graph_detail;
            require(declaration.key && declaration.revision && !declaration.blocks.empty(), "identified nonempty known tensor graph required");
            const auto accession = parent.accession(); const auto& source = accession.declaration();
            stalks = source.source_stalks; guard.dimension(stalks.size()); guard.dimension(declaration.blocks.size());
            original_metric = accession.source_metric();
            std::vector<std::size_t> old_offsets{0}, offsets{0};
            for (const auto& s : stalks) old_offsets.push_back(add(old_offsets.back(),s.directions.size()));
            admit_field(parent.source_field(),old_offsets.back(),guard); const auto values = parent.source_field().values().dense();
            std::map<std::size_t,std::size_t> selected;
            for (std::size_t i = 0; i < declaration.blocks.size(); ++i) {
                guard.step(); const auto& b = declaration.blocks[i];
                require(b.cell.grade == source.source_grade && b.cell.index < stalks.size() && selected.emplace(b.cell.index,i).second,
                    "known tensor graph requires distinct actual source stalks");
            }
            std::vector<numeric::SparseRationalEntry> whole;
            for (std::size_t i = 0; i < stalks.size(); ++i) {
                guard.step(); auto& s = stalks[i]; const auto original_size = s.directions.size();
                std::size_t extent = original_size;
                const auto found = selected.find(i);
                std::vector<numeric::SparseRationalEntry> entries;
                if (found != selected.end()) {
                    const auto& b = declaration.blocks[found->second];
                    require(b.source_basis == s.basis && b.lifted_basis && b.lifted_basis != s.basis,
                        "known tensor graph requires actual source and new lifted basis identities");
                    const auto l = axes(s,b.left,guard), r = axes(s,b.right,guard);
                    const auto count = machine::checked_size_mul(l.size(),r.size()); extent = add(extent,count);
                    guard.dimension(extent); guard.dimension(count); guard.entries(add(count,original_size));
                    require(b.products.size() == count, "known tensor graph omits ordered product directions");
                    std::set<std::uint64_t> names(s.directions.begin(),s.directions.end());
                    for (const auto name : b.products) { guard.step(); require(name && names.insert(name).second, "known tensor product direction zero, repeated or colliding"); }
                    const auto lm = principal(s.metric,l,guard), rm = principal(s.metric,r,guard);
                    Matrix enlarged(extent,extent);
                    for (const auto& [ij,q] : s.metric.entries()) { guard.step(); enlarged.set(ij.first,ij.second,q); }
                    for (const auto& [ij,q] : lm.entries()) for (const auto& [ab,v] : rm.entries()) {
                        guard.step(); enlarged.set(add(original_size,add(machine::checked_size_mul(ij.first,r.size()),ab.first)),
                            add(original_size,add(machine::checked_size_mul(ij.second,r.size()),ab.second)),guard.mul(q,v));
                        guard.entries(enlarged.nonzeros());
                    }
                    for (std::size_t a = 0; a < l.size(); ++a) {
                        const auto& left = values[old_offsets[i]+l[a]]; if (left == 0) continue;
                        for (std::size_t baxis = 0; baxis < r.size(); ++baxis) {
                            guard.step(); const auto& right = values[old_offsets[i]+r[baxis]]; if (right == 0) continue;
                            const auto axis = add(machine::checked_size_mul(a,r.size()),baxis); const auto q = guard.mul(left,right);
                            entries.push_back({axis,q}); whole.push_back({add(offsets.back(),add(original_size,axis)),q});
                        }
                    }
                    products.push_back({s.cell,Tensor(numeric::IndexDomain({l.size(),r.size()}),numeric::StructuralSupport::full(count),
                        numeric::SparseRationalVector(count,std::move(entries)))});
                    s.basis = b.lifted_basis; s.directions.insert(s.directions.end(),b.products.begin(),b.products.end()); s.metric = std::move(enlarged);
                }
                // Original supported zeros and every unselected stalk remain in the field.
                for (std::size_t j = 0; j < original_size; ++j) if (values[old_offsets[i]+j] != 0)
                    whole.push_back({add(offsets.back(),j),values[old_offsets[i]+j]});
                offsets.push_back(add(offsets.back(),extent)); guard.dimension(offsets.back());
                retained = add(retained,add(s.directions.size(),s.metric.nonzeros())); guard.entries(add(retained,whole.size()));
            }
            std::map<std::uint64_t,std::vector<std::uint64_t>> bases;
            for (const auto& s : stalks) {
                guard.step(); const auto [it,inserted] = bases.emplace(s.basis,s.directions);
                require(inserted || it->second == s.directions,"known tensor graph shared basis changes ordered directions");
            }
            std::sort(whole.begin(),whole.end(),[](const auto& a,const auto& b){return a.index < b.index;});
            lifted = Tensor(numeric::IndexDomain({offsets.back()}),numeric::StructuralSupport::full(offsets.back()),
                numeric::SparseRationalVector(offsets.back(),std::move(whole)));
            projection = Matrix(old_offsets.back(),offsets.back()); section = Matrix(offsets.back(),old_offsets.back());
            for (std::size_t i = 0; i < stalks.size(); ++i) for (std::size_t j = 0; j < old_offsets[i+1]-old_offsets[i]; ++j) {
                guard.step(); projection.set(old_offsets[i]+j,offsets[i]+j,1); section.set(offsets[i]+j,old_offsets[i]+j,1);
            }
            metric = block_metric(stalks,guard);
            require(equal(tensor(numeric::checked_apply(projection,lifted.values().dense(),guard),guard),parent.source_field()),
                "known tensor graph loses original source field");
            require(numeric::checked_multiply(metric,section,guard) == numeric::checked_multiply(projection.transpose(),original_metric,guard),
                "known tensor graph projection metric adjoint differs");
            for (const auto* m : {&metric,&original_metric,&projection,&section}) retained = add(retained,m->nonzeros());
            retained = add(retained,lifted.values().nonzeros());
            for (const auto& p : products) retained = add(retained,p.field.values().nonzeros());
            for (const auto& b : declaration.blocks) retained = add(retained,add(5,add(b.left.size(),add(b.right.size(),b.products.size()))));
            guard.entries(retained); guard.step();
        }
    };
public:
    OwnedRelationalKnownTensorGraph(RelationalAccessionInput input, RelationalKnownTensorGraphDeclaration d,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        numeric::ExactActionGuard guard(limits,std::move(check)); state_ = std::make_shared<const State>(std::move(input),std::nullopt,std::move(d),guard);
    }
    OwnedRelationalKnownTensorGraph(OwnedRelationalLiteralInput literal, RelationalKnownTensorGraphDeclaration d,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        numeric::ExactActionGuard guard(limits,std::move(check)); const auto input = literal.input();
        state_ = std::make_shared<const State>(input,std::move(literal),std::move(d),guard);
    }
    RelationalAccessionInput parent() const { return state_->parent; }
    std::optional<OwnedRelationalLiteralInput> literal_parent() const { return state_->literal; }
    bool same_owner(const OwnedRelationalKnownTensorGraph& other) const noexcept { return state_ == other.state_; }
    numeric::ExactActionLimits limits() const { return state_->limits; }
    const RelationalKnownTensorGraphDeclaration& declaration() const & { return state_->declaration; }
    const std::vector<StalkBasisDeclaration>& stalks() const & { return state_->stalks; }
    const Tensor& lifted_field() const & { return state_->lifted; }
    const Matrix& metric() const & { return state_->metric; }
    const Matrix& original_metric() const & { return state_->original_metric; }
    const Matrix& projection() const & { return state_->projection; }
    const Matrix& zero_section() const & { return state_->section; }
    const Tensor& product(core::CellId cell) const & {
        const auto found = std::find_if(state_->products.begin(),state_->products.end(),[&](const auto& p){return p.cell == cell;});
        field_program_detail::require(found != state_->products.end(),"unknown graph product stalk"); return found->field;
    }
    const RelationalKnownTensorGraphDeclaration& declaration() const && = delete;
    const std::vector<StalkBasisDeclaration>& stalks() const && = delete;
    const Tensor& lifted_field() const && = delete;
    const Matrix& metric() const && = delete;
    const Matrix& original_metric() const && = delete;
    const Matrix& projection() const && = delete;
    const Matrix& zero_section() const && = delete;
    const Tensor& product(core::CellId) const && = delete;
    std::size_t retained_entries() const noexcept { return state_->retained; }
    void require_image(const Tensor& field, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        known_tensor_graph_detail::policy(this->limits(),limits); numeric::ExactActionGuard guard(limits,std::move(check)); guard.step();
        field_program_detail::admit_field(field,state_->lifted.domain().logical_size(),guard);
        field_program_detail::require(field_interface_detail::equal(field,state_->lifted),"field is outside actual known-input graph image");
    }
    RelationalKnownTensorGraphInput admit(OwnedRelationalInputAccession target,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};
class RelationalKnownTensorGraphInput {
public:
    OwnedRelationalKnownTensorGraph graph() const { return graph_; }
    RelationalAccessionInput input() const { return input_; }
private:
    friend class OwnedRelationalKnownTensorGraph;
    RelationalKnownTensorGraphInput(OwnedRelationalKnownTensorGraph graph,RelationalAccessionInput input)
        : graph_(std::move(graph)), input_(std::move(input)) {}
    OwnedRelationalKnownTensorGraph graph_;
    RelationalAccessionInput input_;
};
inline RelationalKnownTensorGraphInput OwnedRelationalKnownTensorGraph::admit(OwnedRelationalInputAccession target,
    numeric::ExactActionLimits limits,std::function<void()> check) const {
    using namespace known_tensor_graph_detail; const auto pin = *this; policy(pin.limits(),limits); Guard guard(limits,std::move(check));
    guard.step(); guard.entries(pin.retained_entries()); const auto a = pin.parent().accession(); const auto p = a.program(), q = target.program();
    require(target.asset().same_owner(a.asset()),"known tensor graph requires its actual original asset owner");
    require(q.declaration().key == p.declaration().key && q.declaration().revision > p.declaration().revision,
        "known tensor graph requires a newer explicit program revision");
    const auto& d = target.declaration(); const auto& old = a.declaration();
    require(d.node == old.node && d.source_grade == old.source_grade && d.instances.cells == old.instances.cells &&
        d.instances.occurrences == old.instances.occurrences && d.source_stalks.size() == pin.stalks().size(),
        "known tensor graph changes original accession source or instances");
    for (std::size_t i = 0; i < d.source_stalks.size(); ++i) {
        guard.step(); require(same_stalk(d.source_stalks[i],pin.stalks()[i]),"known tensor graph lifted stalk basis or metric differs");
    }
    const auto before = p.publication(), after = q.publication(); const auto& source = before.endpoint().source; const auto& next = after.endpoint().source;
    require(source.grade_count() == next.grade_count(),"known tensor graph program changes retained grades");
    algebra::RetainedSourceMap identity; identity.cells.resize(source.grade_count()); identity.occurrences.resize(source.grade_count());
    for (std::size_t k = 0; k < source.grade_count(); ++k) {
        const auto g = field_program_detail::grade(k); const auto& s = source.grade(g);
        require(s.cell_count() == next.grade(g).cell_count() && s.incidence_count() == next.grade(g).incidence_count(),"known tensor graph changes original source topology");
        for (std::size_t i = 0; i < s.cell_count(); ++i) identity.cells[k].push_back(static_cast<std::uint32_t>(i));
        for (std::size_t i = 0; i < s.incidence_count(); ++i) identity.occurrences[k].push_back(i);
    }
    field_extension_detail::preserve_source(source,next,identity,guard);
    return {pin,target.admit(pin.parent().use(),pin.lifted_field(),limits,[&]{guard.step();})};
}
} // namespace rfc::inference
