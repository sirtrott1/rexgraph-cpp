#pragma once
#include "rfc/storage/temporal_codec_primitives.hpp"

namespace rfc::storage::temporal_codec_detail {
inline constexpr std::uint32_t relational_codes_flag = 2u;
inline constexpr std::uint32_t relational_code_patch_flag = 4u;
inline void code_value(Writer &w, const RelationalCodeValue &v, std::size_t depth = 0) {
    require(depth < 64, "code wire value nesting limit");
    w.u8(static_cast<std::uint8_t>(v.data.index()));
    if (auto p = std::get_if<numeric::SparseRationalTensor>(&v.data))
        tensor(w, *p);
    else if (auto symbol = std::get_if<std::uint64_t>(&v.data))
        w.u64(*symbol);
    else {
        const auto &product = std::get<2>(v.data);
        w.u64(product.size());
        for (const auto &f : product)
            code_value(w, f, depth + 1);
    }
}
inline RelationalCodeValue code_value(Reader &r, std::size_t depth = 0) {
    require(depth < 64, "code wire value nesting limit");
    const auto tag = r.u8();
    if (tag == 0)
        return RelationalCodeValue{tensor(r)};
    if (tag == 1)
        return RelationalCodeValue{r.u64()};
    require(tag == 2, "unknown code value constructor");
    const auto n = r.count();
    RelationalCodeValue::Product p;
    p.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        p.push_back(code_value(r, depth + 1));
    return RelationalCodeValue{std::move(p)};
}
inline void code_type(Writer &w, const RelationalCodeType &t) {
    require(t.kind <= RelationalCodeTypeKind::Product, "unknown code type constructor");
    w.u64(t.key);
    w.u8(static_cast<std::uint8_t>(t.kind));
    keys(w, t.atoms);
    w.u64(t.factors.size());
    for (const auto &f : t.factors) {
        w.u64(f.key);
        w.u64(f.type);
        w.u16(f.grade);
        string(w, f.source_aspect);
    }
}
inline RelationalCodeType code_type(Reader &r) {
    RelationalCodeType t;
    t.key = r.u64();
    t.kind = static_cast<RelationalCodeTypeKind>(r.u8());
    require(t.kind <= RelationalCodeTypeKind::Product, "unknown code type constructor");
    t.atoms = keys(r);
    const auto n = r.count(26);
    for (std::size_t i = 0; i < n; ++i) {
        const auto key = r.u64(), type = r.u64();
        const auto grade = r.u16();
        t.factors.push_back({key, type, grade, r.string()});
    }
    return t;
}
inline void binary_program(Writer &w, const composite::BinaryProgram &p) {
    w.u16(p.register_count());
    w.u64(p.instruction_count());
    for (const auto &i : p.instructions()) {
        w.u8(static_cast<std::uint8_t>(i.opcode));
        w.u16(i.dst);
        w.u16(i.a);
        w.u16(i.b);
        w.u16(i.c);
        w.u32(i.aspect);
    }
}
inline composite::BinaryProgram binary_program(Reader &r) {
    composite::BinaryProgram p(r.u16());
    const auto n = r.count(13);
    for (std::size_t j = 0; j < n; ++j) {
        composite::BinaryInstruction i;
        i.opcode = static_cast<composite::BinaryOpcode>(r.u8());
        i.dst = r.u16();
        i.a = r.u16();
        i.b = r.u16();
        i.c = r.u16();
        const auto a = r.u32();
        require(a <= std::numeric_limits<composite::AspectId>::max(), "binary aspect overflow");
        i.aspect = static_cast<composite::AspectId>(a);
        p.append(i);
        require(p.instructions().back() == i, "noncanonical binary operand lane");
    }
    return p;
}
inline void code_operation(Writer &w, const RelationalCodeOperation &op) {
    require(op.opcode <= RelationalCodeOpcode::Binary, "unknown relational code opcode");
    w.u64(op.key);
    w.u64(op.input);
    w.u64(op.output);
    w.u8(static_cast<std::uint8_t>(op.opcode));
    w.u8(op.kernel.has_value());
    if (op.kernel)
        tensor(w, *op.kernel);
    keys(w, op.word);
    w.u64(op.legs.size());
    for (const auto &l : op.legs) {
        w.u64(l.input_factor);
        w.u64(l.operation);
    }
    w.u8(op.binary.has_value());
    if (op.binary)
        binary_program(w, *op.binary);
    w.u64(op.registers.size());
    for (auto reg : op.registers)
        w.u16(reg);
}
inline RelationalCodeOperation code_operation(Reader &r) {
    RelationalCodeOperation op;
    op.key = r.u64();
    op.input = r.u64();
    op.output = r.u64();
    op.opcode = static_cast<RelationalCodeOpcode>(r.u8());
    require(op.opcode <= RelationalCodeOpcode::Binary, "unknown relational code opcode");
    if (r.flag())
        op.kernel = tensor(r);
    op.word = keys(r);
    auto n = r.count(16);
    for (std::size_t i = 0; i < n; ++i) {
        const auto f = r.u64(), o = r.u64();
        op.legs.push_back({f, o});
    }
    if (r.flag())
        op.binary = binary_program(r);
    n = r.count(2);
    for (std::size_t i = 0; i < n; ++i)
        op.registers.push_back(r.u16());
    return op;
}
inline void validate_code_section_record(const RelationalCodeSection &p) {
    require(p.key != 0 && p.type != 0 && p.field != 0 && !(p.cell && p.occurrence),
            "invalid code section identity/owner");
    if (p.source_aspects)
        require(p.occurrence && std::holds_alternative<std::uint64_t>(p.value.data) &&
                    std::get<std::uint64_t>(p.value.data) == 0,
                "source-aspect record cannot discard a copied value");
}
inline void code_section(Writer &w, const RelationalCodeSection &p) {
    validate_code_section_record(p);
    w.u64(p.key); w.u64(p.field); w.u64(p.type); w.u64(p.block);
    w.u8(p.cell.has_value()); if (p.cell) cell(w, *p.cell);
    w.u8(p.occurrence.has_value()); if (p.occurrence) occurrence(w, *p.occurrence);
    w.u8(p.source_aspects);
    if (!p.source_aspects) code_value(w, p.value);
}
inline RelationalCodeSection code_section(Reader &r) {
    RelationalCodeSection p;
    p.key = r.u64(); p.field = r.u64(); p.type = r.u64(); p.block = r.u64();
    if (r.flag()) p.cell = cell(r);
    if (r.flag()) p.occurrence = occurrence(r);
    p.source_aspects = r.flag();
    if (!p.source_aspects) p.value = code_value(r);
    validate_code_section_record(p);
    return p;
}
inline void code_interface(Writer &w, const RelationalCodeInterface &p) {
    w.u64(p.key); w.u64(p.left); w.u64(p.right);
    w.u64(p.left_restriction); w.u64(p.right_restriction);
}
inline RelationalCodeInterface code_interface(Reader &r) {
    RelationalCodeInterface p;
    p.key = r.u64(); p.left = r.u64(); p.right = r.u64();
    p.left_restriction = r.u64(); p.right_restriction = r.u64();
    return p;
}
inline void code_descent(Writer &w, const RelationalCodeDescent &p) {
    w.u64(p.key); w.u64(p.interface); w.u64(p.left_operation);
    w.u64(p.right_operation); w.u64(p.interface_operation);
}
inline RelationalCodeDescent code_descent(Reader &r) {
    RelationalCodeDescent p;
    p.key = r.u64(); p.interface = r.u64(); p.left_operation = r.u64();
    p.right_operation = r.u64(); p.interface_operation = r.u64();
    return p;
}
inline void code_state(Writer &w, const RelationalCodeState &s) {
    validate_relational_code_state(s);
    w.u64(s.namespace_key);
    const auto ts = relational_code_detail::index(s.types);
    w.u64(ts.size());
    for (const auto &[id, t] : ts) {
        (void)id;
        code_type(w, *t);
    }
    const auto ops = relational_code_detail::index(s.operations);
    w.u64(ops.size());
    for (const auto &[id, op] : ops) {
        (void)id;
        code_operation(w, *op);
    }
    auto fs = s.fields;
    std::sort(fs.begin(), fs.end());
    keys(w, fs);
    const auto ss = relational_code_detail::index(s.sections);
    w.u64(ss.size());
    for (const auto &[id, p] : ss) {
        (void)id;
        code_section(w, *p);
    }
    const auto is = relational_code_detail::index(s.interfaces);
    w.u64(is.size());
    for (const auto &[id, p] : is) {
        (void)id;
        code_interface(w, *p);
    }
    const auto ds = relational_code_detail::index(s.descents);
    w.u64(ds.size());
    for (const auto &[id, p] : ds) {
        (void)id;
        code_descent(w, *p);
    }
}
inline RelationalCodeState code_state(Reader &r) {
    RelationalCodeState s;
    s.namespace_key = r.u64();
    auto n = r.count(25);
    for (std::size_t i = 0; i < n; ++i)
        s.types.push_back(code_type(r));
    n = r.count(51);
    for (std::size_t i = 0; i < n; ++i)
        s.operations.push_back(code_operation(r));
    s.fields = keys(r);
    n = r.count(35);
    for (std::size_t i = 0; i < n; ++i) {
        s.sections.push_back(code_section(r));
    }
    n = r.count(40);
    for (std::size_t i = 0; i < n; ++i) {
        s.interfaces.push_back(code_interface(r));
    }
    n = r.count(40);
    for (std::size_t i = 0; i < n; ++i) {
        s.descents.push_back(code_descent(r));
    }
    validate_relational_code_state(s);
    return s;
}
// Sparse code patches use a separate RFCXT flag; existing full records retain
// their exact wire layout. Only set-like directories are sorted here.
inline void code_leaf_edit(Writer &w, RelationalCodeLeafEdit edit) {
    require(edit.factors.size() < 64, "code edit path nesting limit");
    keys(w, edit.factors);
    w.u8(static_cast<std::uint8_t>(edit.kind));
    if (edit.kind == RelationalCodeTypeKind::Finite) {
        require(edit.symbol && edit.added_support.empty() && edit.removed_support.empty() &&
                    edit.values.empty(), "invalid finite code edit lanes");
        w.u64(*edit.symbol);
    } else {
        require(edit.kind == RelationalCodeTypeKind::Rational && !edit.symbol,
                "unknown code leaf edit constructor");
        std::sort(edit.added_support.begin(), edit.added_support.end());
        std::sort(edit.removed_support.begin(), edit.removed_support.end());
        keys(w, edit.added_support); keys(w, edit.removed_support);
        std::sort(edit.values.begin(), edit.values.end(),
                  [](const auto &a, const auto &b) { return a.coordinate < b.coordinate; });
        w.u64(edit.values.size());
        for (const auto &v : edit.values) { w.u64(v.coordinate); rational(w, v.target); }
    }
}
inline RelationalCodeLeafEdit code_leaf_edit(Reader &r) {
    RelationalCodeLeafEdit edit;
    edit.factors = keys(r);
    require(edit.factors.size() < 64, "code edit path nesting limit");
    edit.kind = static_cast<RelationalCodeTypeKind>(r.u8());
    if (edit.kind == RelationalCodeTypeKind::Finite)
        edit.symbol = r.u64();
    else {
        require(edit.kind == RelationalCodeTypeKind::Rational, "unknown code leaf edit constructor");
        edit.added_support = keys(r); edit.removed_support = keys(r);
        const auto n = r.count(18);
        for (std::size_t i = 0; i < n; ++i) {
            const auto coordinate = r.u64();
            edit.values.push_back({coordinate, r.rational()});
        }
    }
    return edit;
}
inline void code_section_edit(Writer &w, RelationalCodeSectionEdit edit) {
    w.u64(edit.section); w.u64(edit.type);
    std::sort(edit.leaves.begin(), edit.leaves.end(),
              [](const auto &a, const auto &b) { return a.factors < b.factors; });
    w.u64(edit.leaves.size());
    for (const auto &leaf : edit.leaves) code_leaf_edit(w, leaf);
}
inline RelationalCodeSectionEdit code_section_edit(Reader &r) {
    RelationalCodeSectionEdit edit;
    edit.section = r.u64(); edit.type = r.u64();
    const auto n = r.count(17);
    for (std::size_t i = 0; i < n; ++i) edit.leaves.push_back(code_leaf_edit(r));
    return edit;
}
inline void code_patch(Writer &w, RelationalCodePatch p) {
    const auto sort_keys = [](auto &xs) { std::sort(xs.begin(), xs.end()); };
    const auto sort_targets = [](auto &xs) {
        std::sort(xs.begin(), xs.end(), [](const auto &a, const auto &b) { return a.target.key < b.target.key; });
    };
    sort_keys(p.removed_types); sort_keys(p.removed_operations); sort_keys(p.removed_fields);
    sort_keys(p.removed_sections); sort_keys(p.removed_interfaces); sort_keys(p.removed_descents);
    sort_keys(p.fields);
    std::sort(p.types.begin(), p.types.end(), [](const auto &a, const auto &b) { return a.key < b.key; });
    sort_targets(p.operations); sort_targets(p.sections); sort_targets(p.interfaces); sort_targets(p.descents);
    std::sort(p.section_edits.begin(), p.section_edits.end(),
              [](const auto &a, const auto &b) { return a.section < b.section; });
    w.u64(p.namespace_key); w.u64(p.base_state_hash); w.u64(p.target_state_hash);
    keys(w, p.removed_types); w.u64(p.types.size()); for (const auto &t : p.types) code_type(w, t);
    keys(w, p.removed_operations); w.u64(p.operations.size());
    for (const auto &x : p.operations) { w.u8(x.birth); code_operation(w, x.target); }
    keys(w, p.removed_fields); keys(w, p.fields);
    keys(w, p.removed_sections); w.u64(p.sections.size());
    for (const auto &x : p.sections) { w.u8(x.birth); code_section(w, x.target); }
    w.u64(p.section_edits.size()); for (const auto &x : p.section_edits) code_section_edit(w, x);
    keys(w, p.removed_interfaces); w.u64(p.interfaces.size());
    for (const auto &x : p.interfaces) { w.u8(x.birth); code_interface(w, x.target); }
    keys(w, p.removed_descents); w.u64(p.descents.size());
    for (const auto &x : p.descents) { w.u8(x.birth); code_descent(w, x.target); }
}
inline RelationalCodePatch code_patch(Reader &r) {
    RelationalCodePatch p;
    p.namespace_key = r.u64(); p.base_state_hash = r.u64(); p.target_state_hash = r.u64();
    p.removed_types = keys(r);
    auto n = r.count(25); for (std::size_t i = 0; i < n; ++i) p.types.push_back(code_type(r));
    p.removed_operations = keys(r); n = r.count(52);
    for (std::size_t i = 0; i < n; ++i) {
        const bool birth = r.flag(); p.operations.push_back({birth, code_operation(r)});
    }
    p.removed_fields = keys(r); p.fields = keys(r);
    p.removed_sections = keys(r); n = r.count(36);
    for (std::size_t i = 0; i < n; ++i) {
        const bool birth = r.flag(); p.sections.push_back({birth, code_section(r)});
    }
    n = r.count(24); for (std::size_t i = 0; i < n; ++i) p.section_edits.push_back(code_section_edit(r));
    p.removed_interfaces = keys(r); n = r.count(41);
    for (std::size_t i = 0; i < n; ++i) {
        const bool birth = r.flag(); p.interfaces.push_back({birth, code_interface(r)});
    }
    p.removed_descents = keys(r); n = r.count(41);
    for (std::size_t i = 0; i < n; ++i) {
        const bool birth = r.flag(); p.descents.push_back({birth, code_descent(r)});
    }
    return p;
}
inline void code_update(Writer &w, const RelationalCodeUpdate &update) {
    require(!(update.patch && update.target), "ambiguous full and sparse code update");
    if (update.patch)
        code_patch(w, *update.patch);
    else {
        w.u8(update.target.has_value());
        if (update.target) code_state(w, *update.target);
    }
    auto ts = update.transports;
    std::sort(ts.begin(), ts.end(), [](const auto &a, const auto &b) { return a.section < b.section; });
    w.u64(ts.size());
    for (const auto &t : ts) {
        w.u64(t.section);
        w.u64(t.operation);
    }
}
inline RelationalCodeUpdate code_update(Reader &r, bool sparse = false) {
    RelationalCodeUpdate update;
    if (sparse)
        update.patch = code_patch(r);
    else if (r.flag())
        update.target = code_state(r);
    const auto n = r.count(16);
    for (std::size_t i = 0; i < n; ++i) {
        const auto section = r.u64(), op = r.u64();
        update.transports.push_back({section, op});
    }
    return update;
}
inline std::vector<std::byte> code_state_bytes(const RelationalCodeState &s) {
    Writer w;
    code_state(w, s);
    return w.bytes();
}
inline bool code_state_equal(const std::optional<RelationalCodeState> &a,
                             const std::optional<RelationalCodeState> &b) {
    if (a.has_value() != b.has_value())
        return false;
    return !a || code_state_bytes(*a) == code_state_bytes(*b);
}
} // namespace rfc::storage::temporal_codec_detail
