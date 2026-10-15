"""Reconstruct both Yang–Baxter words from original flags with Fraction arithmetic."""
from collections import Counter
from fractions import Fraction as Q
from itertools import product
import json
import subprocess
import sys
import tempfile
import check_native_product_hexagon as hexagon

WORDS = ((0, 1, 0), (1, 0, 1))


def adjacent(carrier, generator):
    assert generator in (0, 1)
    return [1] if generator == carrier % 2 else [0, 1, 0]


def route(carrier, generators):
    out = []
    for generator in generators:
        for action in adjacent(carrier, generator):
            out.append(action)
            carrier = hexagon.target(carrier, action)
    return out


def three_parity(ids):
    p, q, r = (c[0] for c in ids)
    return (p*q + p*r + q*r) % 2


def verify_types(row, raw, plan, counts):
    sources, _, roots, _, _ = plan
    tokens = [[None] + list(range(s['type_count'])) for s in sources]
    seen = set()
    for w in row['witnesses']:
        original, start = tuple(w['original']), w['source']
        assert original in roots[start] and (original, start) not in seen
        seen.add((original, start))
        assert len(w['routes']) == 2
        endings = []
        for r, generators in enumerate(WORDS):
            carrier, key = start, roots[start][original]
            expected = [[carrier, key]]
            for action in route(carrier, generators):
                if action == 0:
                    pairs = raw['associators'][carrier//2][4]
                    maps = dict((b, a) if carrier % 2 else (a, b) for a, b in pairs)
                else:
                    maps = dict(raw['swaps'][carrier][0])
                key = maps[key]
                carrier = hexagon.target(carrier, action)
                assert key == roots[carrier][original]
                expected.append([carrier, key])
            assert w['routes'][r] == expected
            endings.append((carrier, key))
            counts['complete_type_route_steps'] += len(expected)-1
        assert endings[0] == endings[1]
        order = hexagon.ORDERS[start//2][::-1]
        assert endings[0][0] == 2*hexagon.ORDERS.index(order) + start % 2
        counts['complete_type_braid_witnesses'] += 1
        present = [set(s['cells'][c][1] for c in s['cells']) for s in sources]
        counts['type_witnesses_without_current_cells'] += any(t not in p for t, p in zip(original, present))
    assert seen == set(product(product(*tokens), range(12)))


def verify_cell(row, plan, counts):
    ids = hexagon.verify_cell(row, plan, counts)
    sources = plan[0]
    flags = [hexagon.nested_flags(ids, i, sources) for i in range(12)]
    assert len(row['routes']) == 12
    for start, both in enumerate(row['routes']):
        assert len(both) == 2
        endings = []
        for r, actual in enumerate(both):
            carrier, edges, carriers = start, [], [start]
            actions = route(start, WORDS[r])
            for action in actions:
                edges.append(3*carrier+action)
                carrier = hexagon.target(carrier, action)
                carriers.append(carrier)
            assert actual['edges'] == edges and actual['carriers'] == carriers
            expected = []
            for pos, flag in enumerate(flags[start]):
                original = hexagon.ORDERS[start//2][flag['factor']]
                positions, up, down, mapped = [pos], 0, 0, Q(flag['coefficient'])
                for source, to, action in zip(carriers, carriers[1:], actions):
                    before = flags[source][pos]
                    targets = {(hexagon.ORDERS[to//2][f['factor']], f['position']): j for j, f in enumerate(flags[to])}
                    pos = targets[(original, flag['position'])]
                    after = flags[to][pos]
                    a = hexagon.parity(source, action, before['upper'])
                    b = hexagon.parity(source, action, before['lower'])
                    up, down = up ^ a, down ^ b
                    mapped *= -1 if b else 1
                    positions.append(pos)
                    assert before['occurrence'] == after['occurrence'] and before['aspects'] == after['aspects']
                    assert before['exists'] == after['exists'] and before['factor_coefficient'] == after['factor_coefficient']
                    counts['original_braid_flag_steps'] += 1
                final = flags[carrier][pos]
                direct = (-1 if up else 1) * Q(final['coefficient'])
                assert mapped == direct
                assert up == three_parity(ids)
                assert down == three_parity(hexagon.global_ids(flag['lower'], start))
                expected.append([original, positions, up, down, str(mapped), str(direct)])
                counts['braid_flag_boundary_identities'] += 1
                counts['inactive_braid_flags'] += not flag['exists']
                counts['active_zero_braid_flags'] += flag['exists'] and Q(flag['coefficient']) == 0
            assert actual['flags'] == expected
            endings.append((carrier, [(f[0], f[1][-1], *f[2:]) for f in expected]))
            counts['retained_cell_routes'] += 1
        assert endings[0] == endings[1]
    return ids


def verify_action(row, plan, counts):
    sources, _, _, cells, grades = plan
    start, grade, values = hexagon.field(row['input'])
    assert 0 <= start < 12 and 0 <= grade < len(grades)
    for c in values:
        assert hexagon.global_ids(c, start) in cells and sum(id_[0] for id_ in hexagon.unpack(c, start % 2)) == grade

    def edge(actual, source, before, action):
        to = hexagon.target(source, action)
        after = hexagon.image(before, source, action)
        assert actual['action'] == action and hexagon.field(actual['input']) == (source, grade, before)
        assert hexagon.field(actual['image']) == (to, grade, after)
        assert hexagon.field(actual['back']) == (source, grade, before)
        assert hexagon.image(after, to, action) == before
        if grade:
            b = hexagon.boundary(before, source, sources)
            direct = hexagon.boundary(after, to, sources)
            assert direct == hexagon.image(b, source, action)
            assert hexagon.field(actual['mapped_boundary']) == hexagon.field(actual['image_boundary']) == (to, grade-1, direct)
            counts['primitive_chain_naturality'] += 1
            counts['supported_zero_boundary_coordinates'] += sum(q == 0 for q in direct.values())
            if action == 1:
                counts['unsigned_swap_counterexamples'] += hexagon.image(b, source, action, False) != hexagon.boundary(hexagon.image(before, source, action, False), to, sources)
        else:
            assert actual['mapped_boundary'] is actual['image_boundary'] is None
        counts['primitive_field_witnesses'] += 1
        return to, after

    assert len(row['routes']) == len(row['native_paths']) == 2
    endings = []
    for r, (actual, generators) in enumerate(zip(row['routes'], WORDS)):
        carrier, before = start, values
        assert actual['generators'] == list(generators) and len(actual['path']) == 4 and len(actual['steps']) == 3
        assert hexagon.field(actual['path'][0]) == (carrier, grade, before)
        native = [(carrier, grade, before)]
        for j, (step, generator) in enumerate(zip(actual['steps'], generators)):
            initial_carrier, initial_values = carrier, before
            actions = adjacent(carrier, generator)
            assert step['generator'] == generator and hexagon.field(step['input']) == (carrier, grade, before)
            assert len(step['edges']) == len(actions)
            b = hexagon.boundary(before, carrier, sources) if grade else None
            boundary_path = [(carrier, grade-1, b)] if grade else []
            for e, action in zip(step['edges'], actions):
                source = carrier
                carrier, before = edge(e, carrier, before, action)
                native.append((carrier, grade, before))
                if grade:
                    b = hexagon.image(b, source, action)
                    boundary_path.append((carrier, grade-1, b))
            assert hexagon.field(step['image']) == hexagon.field(actual['path'][j+1]) == (carrier, grade, before)
            assert [hexagon.field(f) for f in step['boundary']] == boundary_path
            if grade:
                direct = hexagon.boundary(before, carrier, sources)
                assert b == direct and hexagon.field(step['image_boundary']) == (carrier, grade-1, b)
                counts['adjacent_chain_naturality'] += 1
            else:
                assert step['image_boundary'] is None
            inverse = [(carrier, grade, before)]
            back_carrier, back = carrier, before
            for action in reversed(actions):
                back = hexagon.image(back, back_carrier, action)
                back_carrier = hexagon.target(back_carrier, action)
                inverse.append((back_carrier, grade, back))
            assert [hexagon.field(f) for f in step['inverse']] == inverse
            assert hexagon.field(step['back']) == (initial_carrier, grade, initial_values) == inverse[-1]
            counts['adjacent_involutions'] += 1
        assert [hexagon.field(f) for f in row['native_paths'][r]] == native
        assert len(native) == (6 if r == start % 2 else 8)
        endings.append((carrier, before))
        counts['canonical_native_steps'] += len(native)-1
    assert endings[0] == endings[1]
    order = hexagon.ORDERS[start//2][::-1]
    final = 2*hexagon.ORDERS.index(order) + start % 2
    expected = {}
    for c, value in values.items():
        ids = hexagon.global_ids(c, start)
        image = hexagon.pack(tuple(ids[j] for j in order), start % 2)
        expected[image] = (-1 if three_parity(ids) else 1) * value
    assert endings[0] == (final, expected)
    if grade > 1:
        dd = hexagon.boundary(hexagon.boundary(values, start, sources), start, sources)
        assert hexagon.field(row['dd']) == (start, grade-2, dd) and all(q == 0 for q in dd.values())
        counts['product_chain_directions'] += 1
    else:
        assert row['dd'] is None
    counts['empty_inputs'] += not values
    counts['supported_zero_inputs'] += sum(q == 0 for q in values.values())
    counts['field_braid_witnesses'] += 1
    return start, grade


def verify(exe):
    counts = Counter(arithmetic='fractions.Fraction')
    plans, raw, seen, actions, typed = {}, {}, {}, Counter(), set()
    with tempfile.TemporaryFile(mode='w+') as errors:
        process = subprocess.Popen([exe, '--records'], text=True, stdout=subprocess.PIPE, stderr=errors)
        try:
            for line in process.stdout:
                row = json.loads(line)
                sample = row['sample']
                if row['phase'] == 'plan':
                    assert sample not in plans
                    sources = hexagon.original_sources(row['sources'], counts)
                    pairs, roots = hexagon.declaration(row, sources, counts)
                    cells = set(product(*(s['cells'] for s in sources)))
                    grades = [sum(sum(c[0] for c in ids) == g for ids in cells) for g in range(sum(len(s['sizes'])-1 for s in sources)+1)]
                    assert row['grade_counts'] == grades
                    plans[sample] = sources, pairs, roots, cells, grades
                    raw[sample], seen[sample] = row, set()
                    counts['complete_constructor_triples'] += len(roots[0])
                    counts['declared_empty_grades'] += sum(n == 0 for n in grades)
                elif row['phase'] == 'types':
                    assert sample not in typed
                    verify_types(row, raw[sample], plans[sample], counts)
                    typed.add(sample)
                elif row['phase'] == 'cell':
                    ids = verify_cell(row, plans[sample], counts)
                    assert ids not in seen[sample]
                    seen[sample].add(ids)
                else:
                    assert row['phase'] == 'action'
                    carrier, grade = verify_action(row, plans[sample], counts)
                    actions[sample, carrier, grade] += 1
            assert process.wait() == 0
            errors.seek(0)
            assert errors.read() == '', 'consumer stderr must be empty'
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()
    assert set(plans) == typed == set(range(16))
    for sample, plan in plans.items():
        assert seen[sample] == plan[3]
        for carrier in range(12):
            for grade in range(len(plan[4])):
                assert actions[sample, carrier, grade] == 2
    counts['plans'] = len(plans)
    for name in ('inactive_braid_flags', 'active_zero_braid_flags', 'unsigned_swap_counterexamples',
                 'type_witnesses_without_current_cells', 'supported_zero_boundary_coordinates'):
        assert counts[name] > 0
    return counts


if __name__ == '__main__':
    print(json.dumps(verify(sys.argv[1]), sort_keys=True))
