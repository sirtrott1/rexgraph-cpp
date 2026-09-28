#!/usr/bin/env python3
"""Independent F2 declaration-extension reader and finite-rational recoding oracle.

Uses the standalone Python F1 codec, not C++ bindings. Reads the exact suffixes
exported by rfc_declaration_temporal_tests; checks their placement in RFCXE/RFCXT,
complete byte re-encoding, stable-value recoding and source-layout program rebinding.
This is not a general RFCX source or temporal-patch parser/security verifier.
"""
from __future__ import annotations
import argparse
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import verify_boolean_layout_reference as f1


class Reader(f1.Reader):
    def blob(self) -> bytes:
        return self.read(self.fixed(8))

    def text(self) -> str:
        return self.blob().decode('utf-8')

    def flag(self) -> bool:
        value = self.fixed(1)
        if value not in (0, 1):
            raise ValueError('invalid Boolean flag')
        return bool(value)

    def owned(self):
        return (self.fixed(2), self.fixed(8)) if self.flag() else None


def u(value: int, width: int = 8) -> bytes:
    return value.to_bytes(width, 'little')


def blob(value: bytes) -> bytes:
    return u(len(value)) + value


def text(value: str) -> bytes:
    return blob(value.encode('utf-8'))


def owned(value) -> bytes:
    return b'\0' if value is None else b'\1' + u(value[0], 2) + u(value[1])


def read_value(r, graph, root):
    width = r.fixed(8)
    bits = f1.Bits(r.blob(), width)
    value = f1.read_value(bits, graph, root)
    if bits.pos != width:
        raise ValueError('trailing value bits')
    return value


def write_value(graph, root, value):
    bits = f1.Output()
    f1.write_value(bits, graph, root, value)
    width, data = bits.finish()
    return u(width) + blob(data)


def read_state(r):
    state = {'namespace': r.fixed(8)}
    root, graph = f1.decode_declarations(r.blob())
    state.update(root=root, graph=graph, values=[], branches=[], layouts=[], codes=[], programs=[])
    for _ in range(r.fixed(8)):
        key, identity, block, branch = r.fixed(8), r.identity(), r.fixed(8), r.fixed(8)
        cell, occurrence = r.owned(), r.owned()
        state['values'].append((key, identity, block, branch, cell, occurrence,
                                read_value(r, graph, identity)))
    for _ in range(r.fixed(8)):
        key, source_type, identity, payload = r.fixed(8), r.text(), r.identity(), r.identity()
        population, semantics, partition = r.fixed(1), r.fixed(1), r.flag()
        roles = [(r.fixed(8), r.text(), r.fixed(8), r.fixed(8)) for _ in range(r.fixed(8))]
        state['branches'].append((key, source_type, identity, payload, population, semantics, partition, roles))
    for _ in range(r.fixed(8)):
        key, identity = r.fixed(8), r.identity()
        factors = [(r.fixed(8), r.text()) for _ in range(r.fixed(8))]
        state['layouts'].append((key, identity, factors))
    state['codes'] = [(r.fixed(8), r.identity()) for _ in range(r.fixed(8))]
    for _ in range(r.fixed(8)):
        key, layout, registers, output = r.fixed(8), r.fixed(8), r.fixed(2), r.fixed(2)
        instructions = [(r.fixed(1), *(r.fixed(2) for _ in range(5))) for _ in range(r.fixed(8))]
        state['programs'].append((key, layout, registers, output, instructions))
    return state


def write_state(s):
    out = bytearray(u(s['namespace']) + blob(f1.encode_declarations(s['root'], s['graph'])))
    values = sorted(s['values'])
    out += u(len(values))
    for key, identity, block, branch, cell, occurrence, value in values:
        out += u(key) + f1.encode_id(identity) + u(block) + u(branch) + owned(cell) + owned(occurrence)
        out += write_value(s['graph'], identity, value)
    out += u(len(s['branches']))
    for key, name, identity, payload, population, semantics, partition, roles in sorted(s['branches']):
        out += u(key) + text(name) + f1.encode_id(identity) + f1.encode_id(payload)
        out += bytes((population, semantics, int(partition))) + u(len(roles))
        for role, aspect, minimum, maximum in roles:
            out += u(role) + text(aspect) + u(minimum) + u(maximum)
    out += u(len(s['layouts']))
    for key, identity, factors in sorted(s['layouts']):
        out += u(key) + f1.encode_id(identity) + u(len(factors))
        for factor, aspect in sorted(factors):
            out += u(factor) + text(aspect)
    out += u(len(s['codes']))
    for key, identity in sorted(s['codes']):
        out += u(key) + f1.encode_id(identity)
    out += u(len(s['programs']))
    for key, layout, registers, output, instructions in sorted(s['programs']):
        out += u(key) + u(layout) + u(registers, 2) + u(output, 2) + u(len(instructions))
        for opcode, *operands in instructions:
            out += u(opcode, 1) + b''.join(u(x, 2) for x in operands)
    return bytes(out)


def read_update(r):
    state = read_state(r) if r.flag() else None
    maps = []
    for _ in range(r.fixed(8)):
        a, b, kind = r.identity(), r.identity(), r.fixed(1)
        if state is None or kind not in (0, 1):
            raise ValueError('invalid recoding')
        fills = []
        for _ in range(r.fixed(8)):
            key = r.fixed(8)
            factors = state['graph'][b][1]
            root = next(child for factor, child, _ in factors if factor == key)
            fills.append((key, read_value(r, state['graph'], root)))
        maps.append((a, b, kind, fills))
    replaced = [r.fixed(8) for _ in range(r.fixed(8))]
    return state, maps, replaced


def write_update(update):
    state, maps, replaced = update
    out = bytearray(b'\0' if state is None else b'\1' + write_state(state))
    out += u(len(maps))
    for a, b, kind, fills in sorted(maps):
        out += f1.encode_id(a) + f1.encode_id(b) + bytes((kind,)) + u(len(fills))
        for key, value in sorted(fills):
            root = next(child for factor, child, _ in state['graph'][b][1] if factor == key)
            out += u(key) + write_value(state['graph'], root, value)
    return bytes(out + u(len(replaced)) + b''.join(u(k) for k in sorted(replaced)))


def recode_value(old_graph, new_graph, maps, identity, value):
    target, kind, fills = maps[identity]
    old_kind, old = old_graph[identity]
    new_kind, new = new_graph[target]
    if old_kind != new_kind:
        raise ValueError('different constructors')
    if old_kind in (0, 1, 2, 3):
        return value  # finite symbols are semantic identities, never ordinal codes
    if old_kind == 4:
        children = {key: (child, part) for (key, child, _), part in zip(old, value[1], strict=True)}
        supplied = dict(fills)
        return ('product', [recode_value(old_graph, new_graph, maps, *children[key])
                            if key in children else supplied[key] for key, _, _ in new])
    if old_kind == 5:
        child = next(child for key, child, _ in old if key == value[1])
        return ('alternative', value[1], recode_value(old_graph, new_graph, maps, child, value[2]))
    if old_kind == 6:
        return ('optional', None if value[1] is None else recode_value(old_graph, new_graph, maps, old, value[1]))
    if old_kind == 7:
        return ('repeated', [recode_value(old_graph, new_graph, maps, old[0], part) for part in value[1]])
    raise ValueError('oracle fixture uses no tensor root; C++ tensor permutations have separate exact tests')


def verify(directory):
    checks, records, states = 0, [], {}
    for name in ('base', 'target', 'wide_base', 'wide_target'):
        path = directory / f'reference_{name}.rfcxds'
        data = path.read_bytes()
        r = Reader(data)
        state = read_state(r)
        r.finish()
        if write_state(state) != data:
            raise ValueError(f'noncanonical {name} state')
        seed_name = {'wide_base': 'wide'}.get(name, name) + '.rfcxe'
        if not (directory / seed_name).read_bytes().endswith(data):
            raise ValueError('export is not the endpoint suffix')
        checks += 2
        states[name] = state
        records.append({'file': path.name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    data = (directory / 'reference_update.rfcxdu').read_bytes()
    r = Reader(data)
    update = read_update(r)
    r.finish()
    if write_update(update) != data or update[0] != states['target'] or update[2]:
        raise ValueError('migration target/replacements differ from pure recoding')
    maps = {a: (b, kind, fills) for a, b, kind, fills in update[1]}
    old, new = states['base'], states['target']
    for before, after in zip(old['values'], new['values'], strict=True):
        if before[0] != after[0] or maps[before[1]][0] != after[1]:
            raise ValueError('value or declaration identity mismatch')
        if recode_value(old['graph'], new['graph'], maps, before[1], before[-1]) != after[-1]:
            raise ValueError('independent recoding differs')
        checks += 2
    if new['values'][0][-1] != ('product', [('optional', Fraction(0)), ('symbol', 30), True]):
        raise ValueError('known exact typed payload')
    checks += 3
    # The source migration is a separate suffix after the declaration update.
    source = (directory / 'reference_source.rfcxsm').read_bytes()
    if not (directory / 'migration.rfcxt').read_bytes().endswith(data + source):
        raise ValueError('migration extension ordering differs')
    # Program operands must follow semantic factor keys, even when output happens to agree.
    a, b = states['wide_base'], states['wide_target']
    old_root, target_root = a['layouts'][0][1], b['layouts'][0][1]
    old_factors, target_factors = a['graph'][old_root][1], b['graph'][target_root][1]
    pa, pb = a['programs'][0], b['programs'][0]
    if pa[:4] != pb[:4] or len(pa[4]) != len(pb[4]):
        raise ValueError('program control flow changed')
    for ia, ib in zip(pa[4], pb[4], strict=True):
        if ia[:5] != ib[:5]:
            raise ValueError('program registers/opcode changed')
        if ia[0] == 0:  # BinaryOpcode::LoadAspect
            if old_factors[ia[5]][0] != target_factors[ib[5]][0]:
                raise ValueError('program operand changed factor identity')
        elif ia != ib:
            raise ValueError('non-load operand changed')
        checks += 1
    if [i[5] for i in pb[4][:2]] != [130, 2]:
        raise ValueError('expected wide operand permutation')
    checks += 2
    return {'status': 'passed', 'checks': checks, 'declaration_states': len(states),
            'recoded_values': len(old['values']), 'ordered_program_instructions': len(pa[4]),
            'records': records, 'scope': 'F2 declaration suffixes and stable-value/program transformations; not complete RFCX/RFCXT parsing'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result = verify(args.directory)
    rendered = json.dumps(result, indent=2) + '\n'
    if args.report:
        args.report.write_text(rendered, encoding='utf-8')
    print(rendered, end='')


if __name__ == '__main__':
    main()
