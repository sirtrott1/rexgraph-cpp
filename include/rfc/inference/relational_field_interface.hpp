#pragma once

#include "rfc/inference/relational_field_program.hpp"
#include "rfc/algebra/owned_gluing.hpp"
#include "rfc/geometry/typed_realization.hpp"

namespace rfc::inference {
struct RelationalByteSpan {
    std::size_t begin{}, end{};
    bool operator==(const RelationalByteSpan&) const = default;
};
struct RelationalSpanUse {
    std::uint64_t key{};
    std::vector<storage::StableCellKey> cells;
    std::vector<storage::StableOccurrenceKey> occurrences;
    std::vector<RelationalByteSpan> spans;
};
struct RelationalInputUseDeclaration {
    std::uint64_t key{}, channel{};
    std::string encoding;
    std::vector<std::byte> bytes;
    std::vector<RelationalSpanUse> uses;
};
struct RelationalInputAccessionDeclaration {
    std::uint64_t key{}, node{};
    core::Grade source_grade;
    std::vector<StalkBasisDeclaration> source_stalks;
    algebra::RetainedSourceMap instances;
    numeric::ExactMatrix accession, reconstruction;
    bool isometric{};
};
namespace field_interface_detail {
using namespace field_program_detail;
inline const FieldProgramNode& node(const OwnedRelationalFieldProgram& program, std::uint64_t key) {
    const auto& nodes = program.declaration().nodes;
    const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) { return n.key == key; });
    require(it != nodes.end(), "interface refers to an absent program node"); return *it;
}
inline const StalkBasisDeclaration& stalk(const OwnedRelationalFieldProgram& program, core::CellId cell) {
    const auto& stalks = program.declaration().stalks;
    const auto it = std::find_if(stalks.begin(), stalks.end(), [&](const auto& s) { return s.cell == cell; });
    require(it != stalks.end(), "interface refers to an absent original stalk"); return *it;
}
inline Matrix block_metric(const std::vector<StalkBasisDeclaration>& stalks, Guard& guard) {
    std::size_t size{};
    for (const auto& s : stalks) size = add(size, s.directions.size());
    guard.dimension(size); Matrix out(size, size); std::size_t offset{};
    for (const auto& s : stalks) {
        guard.matrix(s.metric);
        for (const auto& [ij, q] : s.metric.entries()) { guard.step(); out.set(add(offset, ij.first), add(offset, ij.second), q); }
        offset = add(offset, s.directions.size()); guard.entries(out.nonzeros());
    }
    return out;
}
inline Matrix grade_metric(const OwnedRelationalFieldProgram& program, core::Grade g, Guard& guard) {
    std::vector<StalkBasisDeclaration> stalks;
    for (const auto& s : program.declaration().stalks) if (s.cell.grade == g) stalks.push_back(s);
    return block_metric(stalks, guard);
}
inline void faithful(const Matrix& a, const Matrix& r, Guard& guard) {
    guard.matrix(a); guard.matrix(r);
    require(a.cols() == r.rows() && a.rows() == r.cols(), "faithful interface A/R carriers differ");
    require(numeric::checked_multiply(r, a, guard) == Matrix::identity(a.cols()), "faithful interface requires R A = I");
    // Reuse the existing typed-family contract. Charge its dense stacking work
    // before invocation; all exact product intermediates were checked above.
    const auto slots = machine::checked_size_mul(a.rows(), a.cols());
    guard.entries(add(a.nonzeros(), r.nonzeros()));
    const auto cost = add(machine::checked_size_mul(slots, 2), machine::checked_size_mul(a.nonzeros(), r.nonzeros()));
    for (std::size_t i = 0; i < cost; ++i) guard.step();
    const geometry::ReconstructingTypedFamily family({geometry::ReconstructingTypedView(
        geometry::TypedAccession("declared faithful image", a, Matrix(a.cols(), a.rows())), r)});
    (void)family;
}
inline Tensor act(const Matrix& m, const Tensor& x, Guard& guard) {
    return apply(numeric::PreparedSparseExactMap(m), x, guard);
}
inline bool equal(const Tensor& a, const Tensor& b) { return storage::temporal_endpoint_detail::tensor_equal(a, b); }
inline Tensor tensor(const numeric::RationalVector& values, Guard& guard) {
    guard.vector(values); std::vector<numeric::SparseRationalEntry> entries;
    for (std::size_t i = 0; i < values.size(); ++i) if (values[i] != 0) entries.push_back({i, values[i]});
    return {numeric::IndexDomain({values.size()}), numeric::StructuralSupport::full(values.size()),
        numeric::SparseRationalVector(values.size(), std::move(entries))};
}
} // namespace field_interface_detail

class OwnedRelationalInputUseAsset {
    struct State {
        storage::NativeTemporalSeries history;
        RelationalInputUseDeclaration declaration;
    };
public:
    OwnedRelationalInputUseAsset(storage::NativeTemporalSeries history, RelationalInputUseDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace field_interface_detail;
        const auto frozen = declaration; const auto pin = history;
        Guard guard(limits, std::move(check));
        require(frozen.key && frozen.channel && !frozen.encoding.empty() && !frozen.uses.empty(), "input asset identity/channel/encoding/uses");
        guard.entries(add(add(frozen.bytes.size(), frozen.encoding.size()), frozen.uses.size()));
        const auto publication = pin.tail(); source_values(publication.endpoint().source, guard);
        const storage::StableSourceIndex index(publication.endpoint().source); std::set<std::uint64_t> keys;
        std::size_t retained = add(frozen.bytes.size(), frozen.encoding.size());
        for (const auto& use : frozen.uses) {
            guard.step(); require(use.key && keys.insert(use.key).second && !use.spans.empty() &&
                (!use.cells.empty() || !use.occurrences.empty()), "input use needs unique identity, spans and actual source binding");
            retained = add(retained, add(add(use.cells.size(), use.occurrences.size()), add(use.spans.size(), 1)));
            guard.entries(retained);
            for (const auto cell : use.cells) { guard.step(); require(index.cell(cell).has_value(), "input use cell is absent from its source"); }
            for (const auto occurrence : use.occurrences) { guard.step(); require(index.occurrence(occurrence).has_value(), "input use occurrence is absent from its source"); }
            for (const auto span : use.spans) {
                guard.step(); require(span.begin <= span.end && span.end <= frozen.bytes.size(), "input use byte span exceeds original channel");
            }
        }
        state_ = std::make_shared<const State>(State{pin, frozen});
    }
    storage::NativeTemporalSeries history() const { return state_->history; }
    storage::NativeSeriesEndpoint publication() const { return state_->history.tail(); }
    const RelationalInputUseDeclaration& declaration() const & { return state_->declaration; }
    const RelationalInputUseDeclaration& declaration() const && = delete;
    bool same_owner(const OwnedRelationalInputUseAsset& other) const noexcept { return state_ == other.state_; }
    const RelationalSpanUse& use(std::uint64_t key) const & {
        const auto& uses = state_->declaration.uses;
        const auto it = std::find_if(uses.begin(), uses.end(), [&](const auto& u) { return u.key == key; });
        field_program_detail::require(it != uses.end(), "unknown original input use"); return *it;
    }
    const RelationalSpanUse& use(std::uint64_t) const && = delete;
    std::vector<std::vector<std::byte>> recover(std::uint64_t key, std::size_t max_bytes,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        using namespace field_interface_detail;
        const auto pin = *this; Guard guard(limits, std::move(check)); const auto& d = pin.declaration();
        const auto& spans = pin.use(key).spans; std::size_t size{};
        for (const auto span : spans) { guard.step(); size = add(size, span.end - span.begin); }
        if (size > max_bytes) throw std::length_error("input use bounded byte recovery limit");
        guard.entries(add(size, spans.size())); std::vector<std::vector<std::byte>> result;
        for (const auto span : spans) {
            guard.step(); result.emplace_back(d.bytes.begin() + static_cast<std::ptrdiff_t>(span.begin),
                d.bytes.begin() + static_cast<std::ptrdiff_t>(span.end));
        }
        return result;
    }
private:
    std::shared_ptr<const State> state_;
};

class RelationalAccessionInput;
class OwnedRelationalInputAccession {
    struct State {
        OwnedRelationalInputUseAsset asset;
        OwnedRelationalFieldProgram program;
        RelationalInputAccessionDeclaration declaration;
        numeric::ExactMatrix source_metric, target_metric;
    };
public:
    OwnedRelationalInputAccession(OwnedRelationalInputUseAsset asset, OwnedRelationalFieldProgram program,
        RelationalInputAccessionDeclaration declaration, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace field_interface_detail;
        const auto frozen = declaration; const auto asset_pin = asset; const auto program_pin = program;
        Guard guard(limits, std::move(check)); const auto publication = asset_pin.publication();
        const auto& source = publication.endpoint().source;
        require(frozen.key && node(program_pin, frozen.node).kind == FieldProgramNodeKind::Input, "input accession identity or non-input target");
        require(frozen.source_grade.value < source.grade_count(), "input accession original grade");
        const auto count = source.grade(frozen.source_grade).cell_count(); guard.dimension(count);
        require(frozen.source_stalks.size() == count, "input accession omits original stalks");
        std::map<std::uint64_t, std::vector<std::uint64_t>> bases;
        for (std::size_t i = 0; i < count; ++i) {
            guard.step(); const auto& s = frozen.source_stalks[i];
            require(s.cell == core::CellId{frozen.source_grade, static_cast<std::uint32_t>(i)} && s.basis && !s.directions.empty(), "input accession original basis order/identity");
            std::set<std::uint64_t> directions;
            for (const auto key : s.directions) { guard.step(); require(key && directions.insert(key).second, "input accession basis direction identity"); }
            const auto [it, inserted] = bases.emplace(s.basis, s.directions);
            require(inserted || it->second == s.directions, "input accession shared basis changes ordered directions");
            guard.dimension(s.directions.size()); guard.matrix(s.metric);
            require(s.metric.rows() == s.directions.size() && s.metric.cols() == s.directions.size(), "input accession metric carrier");
            field::hodge_frame_detail::positive(s.metric, guard);
        }
        guard.entries(algebra::owned_gluing_detail::words(frozen.instances));
        const auto target_publication = program_pin.publication();
        algebra::owned_gluing_detail::map(source, target_publication.endpoint().source, frozen.instances, false, guard);
        auto source_metric = block_metric(frozen.source_stalks, guard);
        auto target_metric = grade_metric(program_pin, node(program_pin, frozen.node).output_grade, guard);
        require(frozen.accession.cols() == source_metric.rows() && frozen.accession.rows() == target_metric.rows(), "input accession actual source/model carriers");
        faithful(frozen.accession, frozen.reconstruction, guard);
        if (frozen.isometric) {
            const auto pullback = numeric::checked_multiply(frozen.accession.transpose(),
                numeric::checked_multiply(target_metric, frozen.accession, guard), guard);
            require(pullback == source_metric, "input accession false metric pullback claim");
        }
        state_ = std::make_shared<const State>(State{asset_pin, program_pin, frozen, std::move(source_metric), std::move(target_metric)});
    }
    OwnedRelationalInputUseAsset asset() const { return state_->asset; }
    OwnedRelationalFieldProgram program() const { return state_->program; }
    const RelationalInputAccessionDeclaration& declaration() const & { return state_->declaration; }
    const RelationalInputAccessionDeclaration& declaration() const && = delete;
    const numeric::ExactMatrix& source_metric() const & { return state_->source_metric; }
    const numeric::ExactMatrix& source_metric() const && = delete;
    const numeric::ExactMatrix& target_metric() const & { return state_->target_metric; }
    const numeric::ExactMatrix& target_metric() const && = delete;
    RelationalAccessionInput admit(std::uint64_t use, numeric::SparseRationalTensor source_field,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};
class RelationalAccessionInput {
public:
    OwnedRelationalInputAccession accession() const { return accession_; }
    std::uint64_t use() const noexcept { return use_; }
    const numeric::SparseRationalTensor& source_field() const & { return source_; }
    const numeric::SparseRationalTensor& source_field() const && = delete;
    const numeric::SparseRationalTensor& represented() const & { return represented_; }
    const numeric::SparseRationalTensor& represented() const && = delete;
private:
    friend class OwnedRelationalInputAccession;
    RelationalAccessionInput(OwnedRelationalInputAccession a, std::uint64_t u,
        numeric::SparseRationalTensor source, numeric::SparseRationalTensor represented)
        : accession_(std::move(a)), use_(u), source_(std::move(source)), represented_(std::move(represented)) {}
    OwnedRelationalInputAccession accession_;
    std::uint64_t use_{};
    numeric::SparseRationalTensor source_, represented_;
};
inline RelationalAccessionInput OwnedRelationalInputAccession::admit(std::uint64_t use, numeric::SparseRationalTensor source_field,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace field_interface_detail;
    const auto pin = *this; const auto frozen = source_field; Guard guard(limits, std::move(check));
    const auto& d = pin.declaration(); const auto asset = pin.asset(); (void)asset.use(use);
    admit_field(frozen, d.accession.cols(), guard); auto represented = act(d.accession, frozen, guard);
    const auto reconstructed = act(d.reconstruction, represented, guard);
    require(equal(frozen, reconstructed) && equal(represented, act(d.accession, reconstructed, guard)), "input accession image/reconstruction failure");
    return {pin, use, frozen, std::move(represented)};
}
class OwnedAccessionFieldResult {
    struct State {
        OwnedRelationalFieldResult result;
        std::vector<RelationalAccessionInput> inputs;
    };
public:
    OwnedRelationalFieldResult result() const { return state_->result; }
    const std::vector<RelationalAccessionInput>& inputs() const & { return state_->inputs; }
    const std::vector<RelationalAccessionInput>& inputs() const && = delete;
    bool same_owner(const OwnedAccessionFieldResult& other) const noexcept { return state_ == other.state_; }
private:
    friend OwnedAccessionFieldResult execute_relational_inputs(OwnedRelationalFieldProgram, std::vector<RelationalAccessionInput>, numeric::ExactActionLimits, std::function<void()>);
    explicit OwnedAccessionFieldResult(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
inline OwnedAccessionFieldResult execute_relational_inputs(OwnedRelationalFieldProgram program,
    std::vector<RelationalAccessionInput> inputs, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace field_interface_detail;
    const auto pin = program; const auto frozen = inputs; Guard guard(limits, check);
    guard.entries(frozen.size()); std::vector<RelationalFieldInput> fields;
    for (const auto& input : frozen) {
        guard.step(); const auto accession = input.accession();
        const auto target = accession.program();
        require(pin.fibers().same_owner(target.fibers()), "input accession targets a different actual program owner");
        fields.push_back({accession.declaration().node, input.use(), input.represented()});
    }
    auto result = pin.execute(std::move(fields), limits, std::move(check));
    return OwnedAccessionFieldResult({std::move(result), frozen});
}

struct RelationalNodeCell { std::uint64_t node{}; core::CellId cell; };
struct RelationalFieldInterfaceDeclaration {
    std::uint64_t key{}, branch{};
    std::vector<RelationalNodeCell> cells;
};
struct RelationalFieldViewDeclaration {
    std::uint64_t key{}, basis{};
    std::vector<std::uint64_t> directions;
    numeric::ExactMatrix accession, reconstruction;
    std::optional<numeric::ExactMatrix> physical;
    bool operator==(const RelationalFieldViewDeclaration&) const = default;
};
enum class FieldReadingStatus : std::uint8_t { Derived, Observed, Prospective };
class RelationalFieldViewReading;
class RelationalFieldInterface {
    struct State {
        OwnedAccessionFieldResult parent;
        RelationalFieldInterfaceDeclaration declaration;
        algebra::SubcomplexMask patch;
        numeric::ExactMatrix metric;
        numeric::SparseRationalTensor original;
    };
public:
    RelationalFieldInterface(OwnedAccessionFieldResult parent, RelationalFieldInterfaceDeclaration declaration,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
        using namespace field_interface_detail;
        const auto pin = parent; const auto frozen = declaration; Guard guard(limits, std::move(check));
        const auto result = pin.result(); const auto program = result.program(); const auto owner = program.fibers();
        require(frozen.key && frozen.branch && !frozen.cells.empty(), "field interface needs identity/branch/original patch");
        guard.entries(frozen.cells.size()); algebra::SubcomplexMask patch(owner.fibers().complex());
        std::set<core::CellId> cells; std::vector<StalkBasisDeclaration> stalks; numeric::RationalVector values;
        for (const auto& binding : frozen.cells) {
            guard.step(); require(cells.insert(binding.cell).second && node(program, binding.node).output_grade == binding.cell.grade,
                "field interface duplicate cell or wrong actual node grade");
            const auto& basis = stalk(program, binding.cell); stalks.push_back(basis); patch.select(binding.cell);
            guard.dimension(add(values.size(), basis.directions.size()));
            const auto selected = result.select(binding.node, {binding.cell}, basis.directions.size(), limits, [&] { guard.step(); });
            const auto local = selected.front().field.values().dense(); values.insert(values.end(), local.begin(), local.end());
        }
        local::RelationalLocalCarrier(owner.fibers().complex()).require_closed_patch(patch);
        auto metric = block_metric(stalks, guard); auto original = tensor(values, guard);
        state_ = std::make_shared<const State>(State{pin, frozen, std::move(patch), std::move(metric), std::move(original)});
    }
    OwnedAccessionFieldResult parent() const { return state_->parent; }
    const RelationalFieldInterfaceDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFieldInterfaceDeclaration& declaration() const && = delete;
    const numeric::ExactMatrix& metric() const & { return state_->metric; }
    const numeric::ExactMatrix& metric() const && = delete;
    const numeric::SparseRationalTensor& original() const & { return state_->original; }
    const numeric::SparseRationalTensor& original() const && = delete;
    bool same_owner(const RelationalFieldInterface& other) const noexcept { return state_ == other.state_; }
    local::OwnedLinearSection section(const numeric::SparseRationalTensor& field,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const {
        using namespace field_interface_detail;
        const auto pin = state_; const auto frozen = field; Guard guard(limits, std::move(check));
        admit_field(frozen, pin->metric.rows(), guard); const auto values = frozen.values().dense();
        local::LinearSection section; std::size_t offset{}; const auto program = pin->parent.result().program();
        for (const auto& binding : pin->declaration.cells) {
            guard.step(); const auto n = stalk(program, binding.cell).directions.size();
            section.emplace(binding.cell, numeric::RationalVector(values.begin() + static_cast<std::ptrdiff_t>(offset),
                values.begin() + static_cast<std::ptrdiff_t>(add(offset, n)))); offset = add(offset, n);
        }
        return {program.fibers(), pin->patch, std::move(section)};
    }
    RelationalFieldViewReading observe(RelationalFieldViewDeclaration view,
        numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
    RelationalFieldViewReading admit(RelationalFieldViewDeclaration view, numeric::SparseRationalTensor visible,
        FieldReadingStatus status, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) const;
private:
    std::shared_ptr<const State> state_;
};
class RelationalFieldViewReading {
    struct State {
        RelationalFieldInterface interface;
        RelationalFieldViewDeclaration declaration;
        numeric::SparseRationalTensor visible, reconstructed;
        numeric::ExactMatrix metric;
        FieldReadingStatus status;
    };
public:
    RelationalFieldInterface interface() const { return state_->interface; }
    const RelationalFieldViewDeclaration& declaration() const & { return state_->declaration; }
    const RelationalFieldViewDeclaration& declaration() const && = delete;
    const numeric::SparseRationalTensor& visible() const & { return state_->visible; }
    const numeric::SparseRationalTensor& visible() const && = delete;
    const numeric::SparseRationalTensor& reconstructed() const & { return state_->reconstructed; }
    const numeric::SparseRationalTensor& reconstructed() const && = delete;
    const numeric::ExactMatrix& metric() const & { return state_->metric; }
    const numeric::ExactMatrix& metric() const && = delete;
    FieldReadingStatus status() const noexcept { return state_->status; }
private:
    friend class RelationalFieldInterface;
    explicit RelationalFieldViewReading(State state) : state_(std::make_shared<const State>(std::move(state))) {}
    std::shared_ptr<const State> state_;
};
inline RelationalFieldViewReading RelationalFieldInterface::admit(RelationalFieldViewDeclaration view,
    numeric::SparseRationalTensor visible, FieldReadingStatus status, numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace field_interface_detail;
    const auto pin = *this; const auto frozen = view; const auto field = visible; Guard guard(limits, std::move(check));
    require(status == FieldReadingStatus::Derived || status == FieldReadingStatus::Observed || status == FieldReadingStatus::Prospective,
        "unknown field reading status");
    require(frozen.key && frozen.basis && frozen.accession.cols() == pin.metric().rows() &&
        frozen.directions.size() == frozen.accession.rows(), "field view identity or actual original/visible carriers");
    std::set<std::uint64_t> directions;
    for (const auto key : frozen.directions) { guard.step(); require(key && directions.insert(key).second, "field view direction identity"); }
    faithful(frozen.accession, frozen.reconstruction, guard);
    if (frozen.physical) {
        guard.matrix(*frozen.physical);
        require(frozen.physical->rows() == pin.metric().rows() && frozen.physical->cols() == frozen.accession.rows(), "field view physical map carrier");
    }
    admit_field(field, frozen.accession.rows(), guard); auto reconstructed = act(frozen.reconstruction, field, guard);
    require(equal(field, act(frozen.accession, reconstructed, guard)), "field reading is outside declared accession image");
    if (status == FieldReadingStatus::Derived) require(equal(reconstructed, pin.original()), "derived reading disagrees with its actual parent field");
    auto metric = numeric::checked_multiply(frozen.reconstruction.transpose(),
        numeric::checked_multiply(pin.metric(), frozen.reconstruction, guard), guard);
    return RelationalFieldViewReading({pin, frozen, field, std::move(reconstructed), std::move(metric), status});
}
inline RelationalFieldViewReading RelationalFieldInterface::observe(RelationalFieldViewDeclaration view,
    numeric::ExactActionLimits limits, std::function<void()> check) const {
    using namespace field_interface_detail;
    const auto pin = *this; const auto frozen = view; Guard guard(limits, check);
    auto visible = act(frozen.accession, pin.original(), guard);
    return pin.admit(frozen, std::move(visible), FieldReadingStatus::Derived, limits, std::move(check));
}
enum class FieldSectionKind : std::uint8_t { Compatible, ContributionSum };
class OwnedRelationalFieldSection {
public:
    const std::vector<RelationalFieldViewReading>& readings() const & { return readings_; }
    const std::vector<RelationalFieldViewReading>& readings() const && = delete;
    const local::OwnedLinearSection& section() const & { return section_; }
    const local::OwnedLinearSection& section() const && = delete;
    FieldSectionKind kind() const noexcept { return kind_; }
private:
    friend OwnedRelationalFieldSection assemble_relational_readings(std::vector<RelationalFieldViewReading>, FieldSectionKind, numeric::ExactActionLimits, std::function<void()>);
    OwnedRelationalFieldSection(std::vector<RelationalFieldViewReading> readings, local::OwnedLinearSection section, FieldSectionKind kind)
        : readings_(std::move(readings)), section_(std::move(section)), kind_(kind) {}
    std::vector<RelationalFieldViewReading> readings_;
    local::OwnedLinearSection section_;
    FieldSectionKind kind_;
};
inline OwnedRelationalFieldSection assemble_relational_readings(std::vector<RelationalFieldViewReading> readings,
    FieldSectionKind kind, numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    using namespace field_interface_detail;
    const auto frozen = readings; Guard guard(limits, check);
    require(!frozen.empty() && (kind == FieldSectionKind::Compatible || kind == FieldSectionKind::ContributionSum), "field section needs readings and declared assembly kind");
    guard.entries(frozen.size()); const auto interface = frozen.front().interface();
    std::map<std::uint64_t, const RelationalFieldViewDeclaration*> views;
    std::map<std::uint64_t, const std::vector<std::uint64_t>*> bases;
    std::size_t retained = frozen.size();
    for (const auto& reading : frozen) {
        guard.step(); const auto& d = reading.declaration();
        retained = add(retained, add(add(d.directions.size(), d.accession.nonzeros()), d.reconstruction.nonzeros()));
        retained = add(retained, add(add(reading.visible().values().nonzeros(), reading.reconstructed().values().nonzeros()), reading.metric().nonzeros()));
        if (d.physical) retained = add(retained, d.physical->nonzeros());
        guard.entries(retained);
        const auto [v, inserted] = views.emplace(d.key, &d);
        require(inserted || *v->second == d, "one interface view identity changes its declared maps/basis");
        const auto [b, new_basis] = bases.emplace(d.basis, &d.directions);
        require(new_basis || *b->second == d.directions, "one interface visible basis changes ordered directions");
    }
    auto section = interface.section(frozen.front().reconstructed(), limits, [&] { guard.step(); });
    for (std::size_t i = 1; i < frozen.size(); ++i) {
        guard.step(); require(interface.same_owner(frozen[i].interface()), "field readings have different actual interpretation interfaces");
        const auto next = interface.section(frozen[i].reconstructed(), limits, [&] { guard.step(); });
        section = kind == FieldSectionKind::Compatible ? section.glue_compatible(next) : section.sum_contributions(next);
        for (const auto& [cell, values] : section.values()) { (void)cell; guard.vector(values); }
    }
    guard.step(); return {frozen, std::move(section), kind};
}
inline OwnedRelationalFieldSection glue_relational_readings(std::vector<RelationalFieldViewReading> readings,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    return assemble_relational_readings(std::move(readings), FieldSectionKind::Compatible, limits, std::move(check));
}
inline OwnedRelationalFieldSection sum_relational_contributions(std::vector<RelationalFieldViewReading> readings,
    numeric::ExactActionLimits limits = {}, std::function<void()> check = {}) {
    return assemble_relational_readings(std::move(readings), FieldSectionKind::ContributionSum, limits, std::move(check));
}
} // namespace rfc::inference
