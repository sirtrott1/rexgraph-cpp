"""Reconstruct both nested hexagons and original flags with fractions.Fraction."""
from collections import Counter
from fractions import Fraction as Q
from itertools import permutations, product
import json
import subprocess
import sys
import tempfile


ORDERS = tuple(permutations(range(3)))
PAIRS = ((0, 1), (0, 2), (1, 2))


def frozen(value):
    return tuple(frozen(x) for x in value) if isinstance(value, list) else value


def unpack(cell, side):
    return (cell[0][0], cell[0][1], cell[1]) if side == 0 else (cell[0], cell[1][0], cell[1][1])


def pack(ids, side):
    a, b, c = ids
    return ((a, b), c) if side == 0 else (a, (b, c))


def global_ids(cell, carrier):
    result = [None] * 3
    for role, id_ in zip(ORDERS[carrier // 2], unpack(cell, carrier % 2)):
        result[role] = id_
    return tuple(result)


def target(carrier, action):
    x, y, z = ORDERS[carrier // 2]
    side = carrier % 2
    if action == 0:
        return carrier ^ 1
    if action == 1:
        order = (x, z, y) if side else (y, x, z)
    else:
        assert action == 2
        order = (y, z, x) if side else (z, x, y)
    return 2 * ORDERS.index(order) + (side if action == 1 else 1 - side)


def parity(carrier, action, cell):
    p, q, r = (id_[0] for id_ in unpack(cell, carrier % 2))
    if action == 0:
        return 0
    if action == 1:
        return (q * r if carrier % 2 else p * q) % 2
    assert action == 2
    return (p * (q + r) if carrier % 2 else (p + q) * r) % 2


def image(values, carrier, action, signed=True):
    to = target(carrier, action)
    result = {}
    for c, value in values.items():
        ids = global_ids(c, carrier)
        t = pack(tuple(ids[j] for j in ORDERS[to // 2]), to % 2)
        result[t] = -value if signed and parity(carrier, action, c) else value
    return result


def field(row):
    values = {frozen(c): Q(q) for c, q in row['values']}
    assert len(values) == len(row['values']), 'repeated field coordinate'
    return row['carrier'], row['grade'], values


def table(rows):
    result = {(a, b): key for a, b, key in rows}
    assert len(result) == len(rows) == len(set(result.values()))
    return result


def type_images(rows, expected):
    result = dict(rows)
    assert result == expected and len(result) == len(rows) == len(set(result.values()))


def original_sources(rows, counts):
    for source in rows:
        original = source['cells']
        source['cells'] = {frozen(c): (key, t, Q(scale)) for c, key, t, scale in original}
        assert len(original) == len(source['cells'])
        assert source['sizes'] == [sum(c[0] == g for c in source['cells']) for g in range(len(source['sizes']))]
        fibers = {}
        for g, upper, lower, occurrence, pos, exists, lattice, aspects in source['flags']:
            u, l = (g, upper), (g - 1, lower)
            assert u in source['cells'] and l in source['cells'] and aspects[0] == exists
            value = source['cells'][l][2] * Q(lattice) / source['cells'][u][2]
            word = fibers.setdefault(u, [])
            assert pos == len(word)
            word.append((l, occurrence, pos, exists, value, aspects))
        source['fibers'] = fibers
        for u in source['cells']:
            if u[0] < 2:
                continue
            dd = Counter()
            for l, _, _, exists, q, _ in fibers.get(u, []):
                for v, _, _, active, r, _ in fibers.get(l, []):
                    if exists and active:
                        dd[v] += q * r
            assert all(q == 0 for q in dd.values())
            counts['factor_chain_directions'] += 1
    return rows


def declaration(row, sources, counts):
    tokens = [[None] + list(range(s['type_count'])) for s in sources]
    pairs = {}
    assert len(row['pairs']) == 3 and len(row['associators']) == 6 and len(row['swaps']) == 12
    for (a, b), (ab_rows, ba_rows, images) in zip(PAIRS, row['pairs']):
        ab, ba = table(ab_rows), table(ba_rows)
        assert set(ab) == set(product(tokens[a], tokens[b])) and set(ba) == set(product(tokens[b], tokens[a]))
        type_images(images, {ab[(x, y)]: ba[(y, x)] for x, y in product(tokens[a], tokens[b])})
        pairs[(a, b)], pairs[(b, a)] = ab, ba
        counts['complete_pair_directions'] += len(images)
    roots = [{} for _ in range(12)]
    for p, (x, y, z) in enumerate(ORDERS):
        ab_rows, bc_rows, left_rows, right_rows, images = row['associators'][p]
        ab, bc = table(ab_rows), table(bc_rows)
        assert ab == pairs[(x, y)] and bc == pairs[(y, z)]
        left, right = table(left_rows), table(right_rows)
        assert set(left) == set(product(ab.values(), tokens[z]))
        assert set(right) == set(product(bc.values(), tokens[x]))
        type_images(images, {left[(ab[(a, b)], c)]: right[(bc[(b, c)], a)] for a, b, c in product(tokens[x], tokens[y], tokens[z])})
        for t in product(*tokens):
            roots[2*p][t] = left[(ab[(t[x], t[y])], t[z])]
            roots[2*p+1][t] = right[(bc[(t[y], t[z])], t[x])]
    for i, swaps in enumerate(row['swaps']):
        for action, ts in enumerate(swaps, 1):
            to = target(i, action)
            type_images(ts, {root: roots[to][t] for t, root in roots[i].items()})
            counts['complete_swap_directions'] += len(ts)
        for t, root in roots[i].items():
            # The complete constructor maps commute, including absent cell types.
            current, value = i, root
            for action in [0, 1, 0, 1, 0]:
                to = target(current, action)
                if action == 0:
                    value = roots[to][t]
                else:
                    value = dict(row['swaps'][current][action-1])[value]
                current = to
            assert current == target(i, 2) and value == dict(swaps[1])[root]
            counts['unused_type_hexagons'] += 1
    return pairs, roots


def nested_flags(ids, carrier, sources):
    order, side = ORDERS[carrier // 2], carrier % 2
    local = tuple(ids[j] for j in order)
    p, q, _ = (c[0] for c in local)
    out = []
    for f, (role, id_) in enumerate(zip(order, local)):
        for l, occurrence, pos, exists, value, aspects in sources[role]['fibers'].get(id_, []):
            lower = list(local)
            lower[f] = l
            if side == 0:
                inner = int(f == 1 and p % 2 == 1)
                outer = int(f == 2 and (p + q) % 2 == 1)
                path = [[0, 0], [0, 1], [1]][f]
            else:
                inner = int(f == 2 and q % 2 == 1)
                outer = int(f > 0 and p % 2 == 1)
                path = [[0], [1, 0], [1, 1]][f]
            out.append(dict(upper=pack(local, side), lower=pack(lower, side), path=path, factor=f,
                position=pos, word_position=len(out), occurrence=occurrence, flips=[inner, outer, inner ^ outer],
                exists=exists, factor_coefficient=str(value), coefficient=str(-value if inner ^ outer else value), aspects=aspects))
    return out


def pair_flags(ids, roles, sources):
    out = []
    for side, (id_, role) in enumerate(zip(ids, roles)):
        for l, occurrence, pos, exists, value, aspects in sources[role]['fibers'].get(id_, []):
            lower = list(ids)
            lower[side] = l
            flip = int(side == 1 and ids[0][0] % 2 == 1)
            out.append(dict(upper=ids, lower=tuple(lower), side=side, position=pos, occurrence=occurrence,
                flip=flip, exists=exists, factor_coefficient=str(value), coefficient=str(-value if flip else value), aspects=aspects))
    return out


def word(actual, expected):
    for flag in actual:
        flag['upper'], flag['lower'] = frozen(flag['upper']), frozen(flag['lower'])
    assert actual == expected, 'complete retained flag word differs'


def verify_cell(row, plan, counts):
    sources, pairs, roots, cells, _ = plan
    ids = frozen(row['ids'])
    assert ids in cells
    assert len(row['carriers']) == 12 and len(row['edges']) == 36
    types = tuple(s['cells'][id_][1] for s, id_ in zip(sources, ids))
    identities = [[id_[0], s['cells'][id_][0]] for s, id_ in zip(sources, ids)]
    flags = [nested_flags(ids, i, sources) for i in range(12)]
    for i, c in enumerate(row['carriers']):
        local = tuple(ids[j] for j in ORDERS[i // 2])
        assert c['carrier'] == i and frozen(c['cell']) == pack(local, i % 2)
        assert c['identities'] == identities and c['type'] == roots[i][types]
        word(c['flags'], flags[i])
        counts['actual_carrier_certificates'] += 1
        counts['retained_nested_flags'] += len(flags[i])
    for j, e in enumerate(row['edges']):
        src, action = divmod(j, 3)
        to = target(src, action)
        assert e['source'] == src and e['target'] == to and e['action'] == action
        positions = {(ORDERS[to // 2][f['factor']], f['position']): k for k, f in enumerate(flags[to])}
        expected = []
        for k, flag in enumerate(flags[src]):
            original = ORDERS[src // 2][flag['factor']]
            t = positions[(original, flag['position'])]
            other = flags[to][t]
            upper, lower = parity(src, action, flag['upper']), parity(src, action, flag['lower'])
            mapped = (-1 if lower else 1) * Q(flag['coefficient'])
            direct = (-1 if upper else 1) * Q(other['coefficient'])
            assert mapped == direct
            assert global_ids(flag['lower'], src) == global_ids(other['lower'], to)
            expected.append([original, k, t, upper, lower, str(mapped), str(direct)])
            counts['flag_edge_comparisons'] += 1
            counts['reordered_flags'] += k != t
            counts['inactive_flag_comparisons'] += not flag['exists']
            counts['active_zero_flag_comparisons'] += flag['exists'] and Q(flag['coefficient']) == 0
            counts['odd_edge_flags'] += upper
        assert e['flags'] == expected
        assert sorted(x[2] for x in expected) == list(range(len(flags[to])))
        if action != 1:
            assert e['inner'] is None
        else:
            r = e['inner']
            order, side = ORDERS[src // 2], src % 2
            roles = order[side:side+2]
            a, b = (ids[role] for role in roles)
            source, dest = (a, b), (b, a)
            assert frozen(r['source']) == source and frozen(r['target']) == dest
            assert r['types'] == [pairs[roles][tuple(types[role] for role in roles)], pairs[roles[::-1]][tuple(types[role] for role in roles[::-1])]]
            flip = (a[0] * b[0]) % 2
            assert r['flip'] == flip
            first, second = pair_flags(source, roles, sources), pair_flags(dest, roles[::-1], sources)
            word(r['source_flags'], first)
            word(r['target_flags'], second)
            positions = {(1-f['side'], f['position']): k for k, f in enumerate(second)}
            expected = []
            for k, flag in enumerate(first):
                t = positions[(flag['side'], flag['position'])]
                lower = (flag['lower'][0][0] * flag['lower'][1][0]) % 2
                mapped = (-1 if lower else 1) * Q(flag['coefficient'])
                direct = (-1 if flip else 1) * Q(second[t]['coefficient'])
                assert mapped == direct
                expected.append([k, t, flip, lower, str(mapped), str(direct)])
            assert r['correspondence'] == expected
            # The canonical pair service is explicitly lifted into a nested block.
            for pair_word, nested, owner in [(first, flags[src], src), (second, flags[to], to)]:
                for f in pair_word:
                    local = side + f['side']
                    found = [x for x in nested if x['factor'] == local and x['position'] == f['position']]
                    assert len(found) == 1
                    x = found[0]
                    assert x['flips'][0] == f['flip'] and x['occurrence'] == f['occurrence']
                    assert x['exists'] == f['exists'] and x['aspects'] == f['aspects'] and x['factor_coefficient'] == f['factor_coefficient']
                    counts['canonical_inner_flag_lifts'] += 1
            counts['canonical_inner_receipts'] += 1
        counts['directed_cell_edges'] += 1
    counts['cell_certificates'] += 1
    return ids


def boundary(values, carrier, sources):
    out = {}
    for c, value in values.items():
        for f in nested_flags(global_ids(c, carrier), carrier, sources):
            lower = f['lower']
            out.setdefault(lower, Q(0))
            if f['exists']:
                out[lower] += value * Q(f['coefficient'])
    return out


def verify_action(row, plan, counts):
    sources, _, _, cells, grades = plan
    start, grade, values = field(row['input'])
    assert 0 <= start < 12 and 0 <= grade < len(grades)
    for c in values:
        assert global_ids(c, start) in cells and sum(id_[0] for id_ in unpack(c, start % 2)) == grade

    def edge(actual, src, before, action):
        to = target(src, action)
        after = image(before, src, action)
        assert actual['action'] == action and field(actual['input']) == (src, grade, before)
        assert field(actual['image']) == (to, grade, after)
        assert field(actual['back']) == (src, grade, before) and image(after, to, action) == before
        if grade:
            b = boundary(before, src, sources)
            direct = boundary(after, to, sources)
            assert direct == image(b, src, action)
            assert field(actual['mapped_boundary']) == field(actual['image_boundary']) == (to, grade-1, direct)
            counts['chain_naturality_witnesses'] += 1
            counts['supported_zero_boundary_coordinates'] += sum(q == 0 for q in direct.values())
            if action:
                counts['unsigned_swap_counterexamples'] += image(b, src, action, False) != boundary(image(before, src, action, False), to, sources)
        else:
            assert actual['mapped_boundary'] is actual['image_boundary'] is None
        counts['directed_field_edges'] += 1
        return to, after

    assert len(row['hexagons']) == 2
    for number, h in enumerate(row['hexagons']):
        src = start ^ number
        before = image(values, start, 0) if number else values
        assert len(h['long_path']) == 6 and len(h['edges']) == 5
        assert field(h['long_path'][0]) == (src, grade, before)
        direct_to, direct = edge(h['direct'], src, before, 2)
        for j, action in enumerate([0, 1, 0, 1, 0]):
            src, before = edge(h['edges'][j], src, before, action)
            assert field(h['long_path'][j+1]) == (src, grade, before)
        assert src == direct_to and before == direct
        counts['exact_hexagons'] += 1
    if grade > 1:
        dd = boundary(boundary(values, start, sources), start, sources)
        assert field(row['dd']) == (start, grade-2, dd) and all(q == 0 for q in dd.values())
        counts['product_chain_directions'] += 1
    else:
        assert row['dd'] is None
    counts['empty_inputs'] += not values
    counts['supported_zero_inputs'] += sum(q == 0 for q in values.values())
    counts['field_witnesses'] += 1
    return start, grade


def verify(exe):
    counts = Counter(arithmetic='fractions.Fraction')
    plans, seen, actions = {}, {}, set()
    with tempfile.TemporaryFile(mode='w+') as errors:
        process = subprocess.Popen([exe, '--records'], text=True, stdout=subprocess.PIPE, stderr=errors)
        try:
            for line in process.stdout:
                row = json.loads(line)
                sample = row['sample']
                if row['phase'] == 'plan':
                    assert sample not in plans
                    sources = original_sources(row['sources'], counts)
                    assert len(sources) == 3
                    pairs, roots = declaration(row, sources, counts)
                    cells = set(product(*(s['cells'] for s in sources)))
                    grades = [0] * (sum(len(s['sizes'])-1 for s in sources) + 1)
                    for ids in cells:
                        grades[sum(c[0] for c in ids)] += 1
                    assert grades == row['grade_counts']
                    plans[sample] = sources, pairs, roots, cells, grades
                    seen[sample] = set()
                    counts['plans'] += 1
                    counts['complete_constructor_triples'] += len(roots[0])
                    counts['declared_empty_grades'] += sum(n == 0 for n in grades)
                elif row['phase'] == 'cell':
                    ids = verify_cell(row, plans[sample], counts)
                    assert ids not in seen[sample]
                    seen[sample].add(ids)
                else:
                    assert row['phase'] == 'action'
                    carrier, grade = verify_action(row, plans[sample], counts)
                    actions.add((sample, carrier, grade))
            assert process.wait() == 0
        except BaseException:
            process.kill()
            process.wait()
            raise
        finally:
            process.stdout.close()
        errors.seek(0)
        assert not errors.read(), 'unexpected C++ diagnostics'
    assert set(plans) == set(range(16))
    for sample, (_, _, _, cells, grades) in plans.items():
        assert seen[sample] == cells
        assert all((sample, i, g) in actions for i, g in product(range(12), range(len(grades))))
    assert all(counts[k] > 0 for k in ['declared_empty_grades', 'inactive_flag_comparisons', 'active_zero_flag_comparisons',
        'reordered_flags', 'odd_edge_flags', 'canonical_inner_receipts', 'supported_zero_boundary_coordinates', 'unsigned_swap_counterexamples'])
    return dict(sorted(counts.items()))


if __name__ == '__main__':
    print(json.dumps(verify(sys.argv[1]), sort_keys=True))
