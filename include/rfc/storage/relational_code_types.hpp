#pragma once

#include "rfc/composite/program.hpp"
#include "rfc/storage/temporal_source.hpp"
#include <functional>
#include <variant>

namespace rfc::storage {

// These are supported executable constructors, not a closed list of user types.
// Stable type identities carry ordered coordinates/symbols/factors. Product
// declarations and values remain factorized; their Cartesian size is not formed.
enum class RelationalCodeTypeKind : std::uint8_t { Rational, Finite, Product };
struct RelationalCodeFactor {
    std::uint64_t key{}, type{};
    std::uint16_t grade{};
    // Nonempty only for a declared projection of a retained source aspect.
    std::string source_aspect{};
    bool operator==(const RelationalCodeFactor &) const = default;
};
struct RelationalCodeType {
    std::uint64_t key{};
    RelationalCodeTypeKind kind{RelationalCodeTypeKind::Rational};
    // Rational coordinates or finite symbols, in declared (meaningful) order.
    std::vector<std::uint64_t> atoms;
    std::vector<RelationalCodeFactor> factors;
    bool operator==(const RelationalCodeType &) const = default;
};
struct RelationalCodeValue {
    using Product = std::vector<RelationalCodeValue>;
    std::variant<numeric::SparseRationalTensor, std::uint64_t, Product> data{std::uint64_t{0}};
};
enum class RelationalCodeOpcode : std::uint8_t {
    Identity,
    Linear,
    FiniteMap,
    FactorMap,
    FactorProjection,
    Word,
    Binary
};
struct RelationalCodeFactorMap {
    std::uint64_t input_factor{}, operation{};
    bool operator==(const RelationalCodeFactorMap &) const = default;
};
struct RelationalCodeOperation {
    std::uint64_t key{}, input{}, output{};
    RelationalCodeOpcode opcode{RelationalCodeOpcode::Identity};
    // Linear maps retain numerical and structural kernel support separately.
    std::optional<numeric::SparseRationalTensor> kernel;
    // FiniteMap: one target symbol per input symbol, in input declaration order.
    // Word: operation identities in execution order (first applied first).
    std::vector<std::uint64_t> word;
    // FactorMap: one leg per output factor, in output declaration order.
    // FactorProjection: exactly one leg, yielding its operation's output.
    std::vector<RelationalCodeFactorMap> legs;
    std::optional<composite::BinaryProgram> binary;
    // One output register per Boolean output factor (or one for a leaf).
    std::vector<std::uint16_t> registers;
};
struct RelationalCodeSection {
    std::uint64_t key{}, field{}, type{};
    // Optional retained local context. Block zero denotes the ambient source.
    std::uint64_t block{};
    std::optional<StableCellKey> cell;
    std::optional<StableOccurrenceKey> occurrence;
    // A source view stores no copied bits. Its factor values are read from the
    // bound occurrence; value must be the empty sentinel uint64_t{0}.
    bool source_aspects{};
    RelationalCodeValue value;
};
struct RelationalCodeInterface {
    std::uint64_t key{}, left{}, right{}, left_restriction{}, right_restriction{};
    bool operator==(const RelationalCodeInterface &) const = default;
};
struct RelationalCodeDescent {
    std::uint64_t key{}, interface{}, left_operation{}, right_operation{}, interface_operation{};
    bool operator==(const RelationalCodeDescent &) const = default;
};
struct RelationalCodeState {
    // Distinct namespaces never merge because content or fingerprints agree.
    std::uint64_t namespace_key{};
    std::vector<RelationalCodeType> types;
    std::vector<RelationalCodeOperation> operations;
    std::vector<std::uint64_t> fields;
    std::vector<RelationalCodeSection> sections;
    std::vector<RelationalCodeInterface> interfaces;
    std::vector<RelationalCodeDescent> descents;
};
struct RelationalCodeTransport {
    std::uint64_t section{}, operation{}; // operation in target declaration set
    bool operator==(const RelationalCodeTransport &) const = default;
};
// Sparse edits are reconstructive storage records, not field innovations.
// Stable coordinate/factor identities come from the unchanged type declaration.
struct RelationalCodeCoordinateEdit {
    std::uint64_t coordinate{};
    numeric::Rational target{}; // zero explicitly clears the numerical value
};
struct RelationalCodeLeafEdit {
    std::vector<std::uint64_t> factors; // ordered path of stable product factor keys
    RelationalCodeTypeKind kind{RelationalCodeTypeKind::Rational};
    std::vector<std::uint64_t> added_support, removed_support;
    std::vector<RelationalCodeCoordinateEdit> values;
    std::optional<std::uint64_t> symbol;
};
struct RelationalCodeSectionEdit {
    std::uint64_t section{}, type{};
    std::vector<RelationalCodeLeafEdit> leaves;
};
template <class T> struct RelationalCodeReplacement {
    bool birth{};
    T target;
};
struct RelationalCodePatch {
    std::uint64_t namespace_key{}, base_state_hash{}, target_state_hash{};
    std::vector<std::uint64_t> removed_types, removed_operations, removed_fields,
        removed_sections, removed_interfaces, removed_descents;
    // Persistent type definitions are immutable: types here are births only.
    std::vector<RelationalCodeType> types;
    std::vector<std::uint64_t> fields;
    std::vector<RelationalCodeReplacement<RelationalCodeOperation>> operations;
    std::vector<RelationalCodeReplacement<RelationalCodeSection>> sections;
    std::vector<RelationalCodeSectionEdit> section_edits;
    std::vector<RelationalCodeReplacement<RelationalCodeInterface>> interfaces;
    std::vector<RelationalCodeReplacement<RelationalCodeDescent>> descents;
};
enum class RelationalCodeEncoding : std::uint8_t { Automatic, Full, Sparse };
struct RelationalCodeUpdate {
    // Null target removes the code layer explicitly. A replacement also retains
    // changes invisible on the current numerical field (e.g. an unused column).
    std::optional<RelationalCodeState> target;
    std::vector<RelationalCodeTransport> transports;
    // Exactly one representation: patch forbids target. With neither, remove
    // the code layer. An absent update still means inherit the layer unchanged.
    std::optional<RelationalCodePatch> patch{};
};

namespace relational_code_detail {
using temporal_source_detail::require;
template <class T> inline auto index(const std::vector<T> &items) {
    std::map<std::uint64_t, const T *> out;
    for (const auto &x : items)
        require(x.key != 0 && out.emplace(x.key, &x).second, "zero/duplicate relational code identity");
    return out;
}
inline bool tensor_equal(const numeric::SparseRationalTensor &a, const numeric::SparseRationalTensor &b) {
    if (a.domain() != b.domain() || a.support().count() != b.support().count() ||
        a.values().nonzeros() != b.values().nonzeros())
        return false;
    bool same = true;
    a.support().for_each_active([&](auto i) {
        if (!b.support().contains(i))
            same = false;
    });
    const auto &av = a.values().entries();
    const auto &bv = b.values().entries();
    for (std::size_t i = 0; i < av.size(); ++i)
        if (av[i].index != bv[i].index || av[i].value != bv[i].value)
            return false;
    return same;
}
inline bool value_equal(const RelationalCodeValue &a, const RelationalCodeValue &b) {
    if (a.data.index() != b.data.index())
        return false;
    if (auto p = std::get_if<numeric::SparseRationalTensor>(&a.data))
        return tensor_equal(*p, std::get<0>(b.data));
    if (auto p = std::get_if<std::uint64_t>(&a.data))
        return *p == std::get<1>(b.data);
    const auto &av = std::get<2>(a.data);
    const auto &bv = std::get<2>(b.data);
    if (av.size() != bv.size())
        return false;
    for (std::size_t i = 0; i < av.size(); ++i)
        if (!value_equal(av[i], bv[i]))
            return false;
    return true;
}
inline std::size_t factor_index(const RelationalCodeType &type, std::uint64_t key) {
    for (std::size_t i = 0; i < type.factors.size(); ++i)
        if (type.factors[i].key == key)
            return i;
    throw std::invalid_argument("unknown input factor identity");
}
inline bool boolean(const RelationalCodeType &t) {
    return t.kind == RelationalCodeTypeKind::Finite && t.atoms == std::vector<std::uint64_t>{0, 1};
}
inline void unique_atoms(const std::vector<std::uint64_t> &atoms) {
    std::set<std::uint64_t> seen;
    for (auto id : atoms)
        require(seen.insert(id).second, "duplicate ordered code atom");
}
} // namespace relational_code_detail

// An immutable, owning validated declaration set. Evaluation never borrows
// pointers into caller-mutable definitions or temporary endpoints.
class RelationalCodeAlgebra {
  public:
    explicit RelationalCodeAlgebra(const RelationalCodeState &state)
        : types_(state.types), operations_(state.operations) {
        validate();
    }
    RelationalCodeAlgebra(const RelationalCodeAlgebra &) = delete;
    RelationalCodeAlgebra &operator=(const RelationalCodeAlgebra &) = delete;
    // Keep the indexes and their pointees stationary.
    RelationalCodeAlgebra(RelationalCodeAlgebra &&) = delete;
    RelationalCodeAlgebra &operator=(RelationalCodeAlgebra &&) = delete;
    const RelationalCodeType &type(std::uint64_t id) const {
        return *types_by_key_.at(id);
    }
    const RelationalCodeOperation &operation(std::uint64_t id) const {
        return *ops_by_key_.at(id);
    }
    void validate_value(std::uint64_t type_key, const RelationalCodeValue &value) const {
        check_value(type_key, value, 0);
    }
    RelationalCodeValue apply(std::uint64_t operation_key, const RelationalCodeValue &value,
                              std::size_t work_budget = 1000000) const {
        const auto &op = operation(operation_key);
        validate_value(op.input, value);
        auto out = evaluate(op, value, work_budget);
        validate_value(op.output, out);
        return out;
    }

  private:
    void validate();
    void check_value(std::uint64_t, const RelationalCodeValue &, std::size_t) const;
    RelationalCodeValue evaluate(const RelationalCodeOperation &, const RelationalCodeValue &,
                                 std::size_t &) const;
    std::vector<RelationalCodeType> types_;
    std::vector<RelationalCodeOperation> operations_;
    std::map<std::uint64_t, const RelationalCodeType *> types_by_key_;
    std::map<std::uint64_t, const RelationalCodeOperation *> ops_by_key_;
};

inline void RelationalCodeAlgebra::validate() {
    using namespace relational_code_detail;
    types_by_key_ = index(types_);
    ops_by_key_ = index(operations_);
    std::map<std::uint64_t, std::size_t> type_depth, op_depth;
    std::set<std::uint64_t> visiting;
    std::function<std::size_t(std::uint64_t, std::size_t)> visit_type = [&](auto key,
                                                                            auto stack) -> std::size_t {
        require(stack < 64, "code type nesting limit");
        if (type_depth.contains(key))
            return type_depth.at(key);
        require(visiting.insert(key).second, "cyclic code type declaration");
        const auto &t = type(key);
        std::size_t depth = 1;
        require(t.kind <= RelationalCodeTypeKind::Product, "unknown code type constructor");
        if (t.kind == RelationalCodeTypeKind::Product) {
            require(t.atoms.empty(), "product has undeclared atom lane");
            std::set<std::uint64_t> factors;
            for (const auto &f : t.factors) {
                require(f.key != 0 && factors.insert(f.key).second, "zero/duplicate code factor identity");
                depth = std::max(depth, 1 + visit_type(f.type, stack + 1));
            }
        } else {
            require(t.factors.empty(), "leaf type has product factors");
            unique_atoms(t.atoms);
        }
        require(depth <= 64, "code type nesting limit");
        visiting.erase(key);
        return type_depth.emplace(key, depth).first->second;
    };
    for (const auto &t : types_)
        (void)visit_type(t.key, 0);
    visiting.clear();
    std::function<std::size_t(std::uint64_t, std::size_t)> visit_op = [&](auto key,
                                                                          auto stack) -> std::size_t {
        require(stack < 64, "code operation nesting limit");
        if (op_depth.contains(key))
            return op_depth.at(key);
        require(visiting.insert(key).second, "cyclic code operation declaration");
        const auto &op = operation(key);
        const auto &in = type(op.input);
        const auto &out = type(op.output);
        require(op.opcode <= RelationalCodeOpcode::Binary, "unknown relational code opcode");
        require(op.kernel.has_value() == (op.opcode == RelationalCodeOpcode::Linear),
                "unexpected/missing linear kernel");
        require(op.binary.has_value() == (op.opcode == RelationalCodeOpcode::Binary),
                "unexpected/missing binary program");
        if (op.opcode != RelationalCodeOpcode::FiniteMap && op.opcode != RelationalCodeOpcode::Word)
            require(op.word.empty(), "unexpected code word lane");
        if (op.opcode != RelationalCodeOpcode::FactorMap &&
            op.opcode != RelationalCodeOpcode::FactorProjection)
            require(op.legs.empty(), "unexpected factor-map lane");
        if (op.opcode != RelationalCodeOpcode::Binary)
            require(op.registers.empty(), "unexpected register lane");
        std::size_t depth = 1;
        const auto dependency = [&](auto id) { depth = std::max(depth, 1 + visit_op(id, stack + 1)); };
        switch (op.opcode) {
        case RelationalCodeOpcode::Identity:
            require(op.input == op.output, "identity changes declared type");
            break;
        case RelationalCodeOpcode::Linear:
            require(in.kind == RelationalCodeTypeKind::Rational && out.kind == in.kind,
                    "linear map requires rational carriers");
            require(op.kernel->domain() == numeric::IndexDomain({out.atoms.size(), in.atoms.size()}),
                    "code kernel shape mismatch");
            break;
        case RelationalCodeOpcode::FiniteMap:
            require(in.kind == RelationalCodeTypeKind::Finite && out.kind == in.kind &&
                        op.word.size() == in.atoms.size(),
                    "finite map is not total");
            for (auto symbol : op.word)
                require(std::find(out.atoms.begin(), out.atoms.end(), symbol) != out.atoms.end(),
                        "finite map leaves target domain");
            break;
        case RelationalCodeOpcode::Word: {
            auto current = op.input;
            for (auto id : op.word) {
                dependency(id);
                const auto &term = operation(id);
                require(term.input == current, "ill-typed operation word");
                current = term.output;
            }
            require(current == op.output, "operation word output type mismatch");
            break;
        }
        case RelationalCodeOpcode::FactorMap:
        case RelationalCodeOpcode::FactorProjection:
            require(in.kind == RelationalCodeTypeKind::Product, "factor map input is not a product");
            if (op.opcode == RelationalCodeOpcode::FactorMap)
                require(out.kind == RelationalCodeTypeKind::Product && op.legs.size() == out.factors.size(),
                        "factor map target arity");
            else
                require(op.legs.size() == 1, "projection needs one factor leg");
            for (std::size_t i = 0; i < op.legs.size(); ++i) {
                const auto &leg = op.legs[i];
                dependency(leg.operation);
                const auto &term = operation(leg.operation);
                require(term.input == in.factors[factor_index(in, leg.input_factor)].type &&
                            term.output == (op.opcode == RelationalCodeOpcode::FactorMap ? out.factors[i].type
                                                                                         : op.output),
                        "factor leg type mismatch");
            }
            break;
        case RelationalCodeOpcode::Binary: {
            require(in.kind == RelationalCodeTypeKind::Product &&
                        in.factors.size() <= std::numeric_limits<composite::AspectId>::max(),
                    "binary input must be an addressable Boolean product");
            for (const auto &f : in.factors)
                require(boolean(type(f.type)), "binary input factor is not declared Boolean");
            if (out.kind == RelationalCodeTypeKind::Product) {
                for (const auto &f : out.factors)
                    require(boolean(type(f.type)), "binary output factor is not Boolean");
                require(op.registers.size() == out.factors.size(), "binary output arity mismatch");
            } else
                require(boolean(out) && op.registers.size() == 1, "binary output is not Boolean");
            for (auto r : op.registers)
                require(r < op.binary->register_count(), "binary output register absent");
            for (const auto &i : op.binary->instructions())
                if (i.opcode == composite::BinaryOpcode::LoadAspect)
                    require(i.aspect < in.factors.size(), "binary aspect does not name an input factor");
            break;
        }
        }
        require(depth <= 64, "code operation nesting limit");
        visiting.erase(key);
        return op_depth.emplace(key, depth).first->second;
    };
    for (const auto &op : operations_)
        (void)visit_op(op.key, 0);
}

inline void RelationalCodeAlgebra::check_value(std::uint64_t key, const RelationalCodeValue &v,
                                               std::size_t depth) const {
    using namespace relational_code_detail;
    require(depth < 64, "code value nesting limit");
    const auto &t = type(key);
    if (t.kind == RelationalCodeTypeKind::Rational) {
        auto p = std::get_if<numeric::SparseRationalTensor>(&v.data);
        require(p && p->domain() == numeric::IndexDomain({t.atoms.size()}),
                "rational code field domain mismatch");
    } else if (t.kind == RelationalCodeTypeKind::Finite) {
        auto p = std::get_if<std::uint64_t>(&v.data);
        require(p && std::find(t.atoms.begin(), t.atoms.end(), *p) != t.atoms.end(),
                "finite code value outside domain");
    } else {
        auto p = std::get_if<RelationalCodeValue::Product>(&v.data);
        require(p && p->size() == t.factors.size(), "factorized value shape mismatch");
        for (std::size_t i = 0; i < p->size(); ++i)
            check_value(t.factors[i].type, (*p)[i], depth + 1);
    }
}
inline RelationalCodeValue RelationalCodeAlgebra::evaluate(const RelationalCodeOperation &op,
                                                           const RelationalCodeValue &v,
                                                           std::size_t &budget) const {
    using namespace relational_code_detail;
    const auto spend = [&](std::size_t n) {
        require(n <= budget, "code evaluation work budget exhausted");
        budget -= n;
    };
    spend(1);
    switch (op.opcode) {
    case RelationalCodeOpcode::Identity:
        return v;
    case RelationalCodeOpcode::Word: {
        auto out = v;
        for (auto id : op.word)
            out = evaluate(operation(id), out, budget);
        return out;
    }
    case RelationalCodeOpcode::FiniteMap: {
        const auto &atoms = type(op.input).atoms;
        const auto found = std::find(atoms.begin(), atoms.end(), std::get<1>(v.data));
        return RelationalCodeValue{op.word.at(static_cast<std::size_t>(found - atoms.begin()))};
    }
    case RelationalCodeOpcode::FactorMap:
    case RelationalCodeOpcode::FactorProjection: {
        RelationalCodeValue::Product out;
        for (const auto &leg : op.legs)
            out.push_back(evaluate(operation(leg.operation),
                                   std::get<2>(v.data).at(factor_index(type(op.input), leg.input_factor)),
                                   budget));
        if (op.opcode == RelationalCodeOpcode::FactorProjection)
            return out.front();
        return RelationalCodeValue{std::move(out)};
    }
    case RelationalCodeOpcode::Binary: {
        RelationalCodeValue::Product out;
        for (auto r : op.registers) {
            spend(op.binary->instruction_count());
            const auto bit = op.binary->execute_domain_word_with_provider(1, 0, r, [&](auto a, auto) {
                return std::get<std::uint64_t>(std::get<2>(v.data).at(a).data);
            });
            out.push_back(RelationalCodeValue{bit});
        }
        if (type(op.output).kind != RelationalCodeTypeKind::Product)
            return out.front();
        return RelationalCodeValue{std::move(out)};
    }
    case RelationalCodeOpcode::Linear: {
        const auto &k = *op.kernel;
        const auto &x = std::get<0>(v.data);
        const auto rows = type(op.output).atoms.size(), cols = type(op.input).atoms.size();
        std::set<std::size_t> active;
        std::map<std::size_t, numeric::Rational> sums;
        spend(k.support().count());
        k.support().for_each_active([&](auto ij) {
            if (x.support().contains(ij % cols))
                active.insert(ij / cols);
        });
        spend(k.values().nonzeros());
        for (const auto &entry : k.values().entries()) {
            const auto coord = entry.index % cols;
            const auto &es = x.values().entries();
            const auto it = std::lower_bound(es.begin(), es.end(), coord,
                                             [](const auto &a, auto b) { return a.index < b; });
            if (it != es.end() && it->index == coord)
                sums[entry.index / cols] += entry.value * it->value;
        }
        std::vector<numeric::SparseRationalEntry> entries;
        for (auto &[i, q] : sums)
            if (q != 0)
                entries.push_back({i, std::move(q)});
        return RelationalCodeValue{numeric::SparseRationalTensor(
            numeric::IndexDomain({rows}),
            numeric::StructuralSupport::sparse(rows, {active.begin(), active.end()}),
            numeric::SparseRationalVector(rows, std::move(entries)))};
    }
    }
    throw std::invalid_argument("unknown relational code opcode");
}

inline void validate_relational_code_state(const RelationalCodeState &state) {
    using namespace relational_code_detail;
    require(state.namespace_key != 0, "zero code namespace identity");
    RelationalCodeAlgebra algebra(state);
    std::set<std::uint64_t> fields;
    for (auto id : state.fields)
        require(id != 0 && fields.insert(id).second, "zero/duplicate code field identity");
    const auto sections = index(state.sections);
    const auto interfaces = index(state.interfaces);
    (void)index(state.descents);
    for (const auto &s : state.sections) {
        require(fields.contains(s.field) && !(s.cell && s.occurrence), "code section field/owner mismatch");
        if (s.source_aspects) {
            require(s.occurrence && !s.cell && std::holds_alternative<std::uint64_t>(s.value.data) &&
                        std::get<std::uint64_t>(s.value.data) == 0,
                    "source-aspect view has copied/invalid state");
            const auto &t = algebra.type(s.type);
            require(t.kind == RelationalCodeTypeKind::Product, "source-aspect view needs factor declaration");
            std::set<std::string> names;
            for (const auto &f : t.factors)
                require(!f.source_aspect.empty() && names.insert(f.source_aspect).second &&
                            boolean(algebra.type(f.type)),
                        "source-aspect view factor declaration invalid");
        } else
            algebra.validate_value(s.type, s.value);
    }
    for (const auto &i : state.interfaces) {
        const auto &left = *sections.at(i.left);
        const auto &right = *sections.at(i.right);
        const auto &l = algebra.operation(i.left_restriction);
        const auto &r = algebra.operation(i.right_restriction);
        require(left.field == right.field && l.input == left.type && r.input == right.type &&
                    l.output == r.output,
                "ill-typed interface or independently identified fields");
    }
    for (const auto &d : state.descents) {
        const auto &i = *interfaces.at(d.interface);
        const auto &l = algebra.operation(d.left_operation);
        const auto &r = algebra.operation(d.right_operation);
        const auto &k = algebra.operation(d.interface_operation);
        const auto endomorphism = [](const auto &op, auto key) {
            return op.input == key && op.output == key;
        };
        require(endomorphism(l, sections.at(i.left)->type) && endomorphism(r, sections.at(i.right)->type) &&
                    endomorphism(k, algebra.operation(i.left_restriction).output),
                "ill-typed operation descent");
    }
}

// Type identity denotes an executable declaration, not an interpretation string.
// New declarations may be added; changing a shared declaration requires a new ID.
inline void validate_relational_code_transition(const std::optional<RelationalCodeState> &old,
                                                const std::optional<RelationalCodeState> &next,
                                                const std::vector<RelationalCodeTransport> &transports) {
    using namespace relational_code_detail;
    if (!old || !next) {
        require(transports.empty(), "code transports require both endpoints");
        return;
    }
    require(old->namespace_key == next->namespace_key,
            "code namespace migration requires explicit removal/birth");
    const auto ot = index(old->types), nt = index(next->types);
    for (const auto &[id, t] : ot)
        if (nt.contains(id))
            require(*t == *nt.at(id), "shared type identity was redefined");
    const auto os = index(old->sections), ns = index(next->sections);
    RelationalCodeAlgebra algebra(*next);
    std::map<std::uint64_t, std::uint64_t> maps;
    for (const auto &t : transports) {
        require(maps.emplace(t.section, t.operation).second && os.contains(t.section) &&
                    ns.contains(t.section),
                "duplicate or nonpersistent code transport");
        const auto &op = algebra.operation(t.operation);
        require(nt.contains(os.at(t.section)->type) &&
                    *nt.at(os.at(t.section)->type) == *ot.at(os.at(t.section)->type) &&
                    op.input == os.at(t.section)->type && op.output == ns.at(t.section)->type,
                "code transport type mismatch");
    }
    for (const auto &[id, s] : ns)
        if (os.contains(id)) {
            require(os.at(id)->field == s->field, "persistent section cannot change field identity");
            require(os.at(id)->type == s->type || maps.contains(id),
                    "changed code carrier requires an explicit transport");
        }
}

} // namespace rfc::storage
