"""Check the five binary trees with independent recursive Fraction arithmetic."""
from collections import Counter
from fractions import Fraction as Q
from itertools import product
import json
import subprocess
import sys

TREES = [(((0, 1), 2), 3), ((0, (1, 2)), 3), (0, ((1, 2), 3)),
         (0, (1, (2, 3))), ((0, 1), (2, 3))]
EDGES = [(0, 1), (1, 2), (2, 3), (0, 4), (4, 3)]


def freeze(x):
    return tuple(freeze(v) for v in x) if isinstance(x, list) else x


def pack(tree, ids):
    return ids[tree] if isinstance(tree, int) else tuple(pack(t, ids) for t in tree)


def unpack(tree, cell):
    out = [None] * 4

    def walk(t, c):
        if isinstance(t, int):
            assert len(c) == 2
            out[t] = c
        else:
            assert len(c) == 2
            walk(t[0], c[0])
            walk(t[1], c[1])
    walk(tree, cell)
    return tuple(out)


def field(data):
    out = {freeze(c): Q(q) for c, q in data}
    assert len(out) == len(data)
    return out


def pairs(data, left, right):
    result = {(a, b): key for a, b, key in data}
    assert len(result) == len(data) == len(set(result.values()))
    assert set(result) == set(product(left, right))
    return result


def verify(exe):
    done = subprocess.run([exe, '--records'], capture_output=True, text=True, check=True)
    assert not done.stderr
    counts = Counter(arithmetic='fractions.Fraction')
    plans, seen = {}, {}
    for line in done.stdout.splitlines():
        row = json.loads(line)
        sample = row['sample']
        if row['phase'] == 'plan':
            assert sample not in plans
            sources = row['sources']
            assert len(sources) == 4
            for source in sources:
                cells = source['cells']
                source['cells'] = {freeze(c): (key, t, Q(scale)) for c, key, t, scale in cells}
                assert len(source['cells']) == len(cells)
                fibers = {}
                for g, u, l, occurrence, pos, active, lattice, aspects in source['flags']:
                    upper, lower = (g, u), (g - 1, l)
                    assert upper in source['cells'] and lower in source['cells']
                    assert active == aspects[0]
                    coefficient = source['cells'][lower][2] * Q(lattice) / source['cells'][upper][2]
                    word = fibers.setdefault(upper, [])
                    assert pos == len(word)
                    word.append((lower, occurrence, pos, active, coefficient, aspects))
                source['fibers'] = fibers
                for cell in source['cells']:
                    if cell[0] < 2:
                        continue
                    dd = Counter()
                    for lower, _, _, active, q, _ in fibers.get(cell, []):
                        for target, _, _, enabled, r, _ in fibers.get(lower, []):
                            if active and enabled:
                                dd[target] += q * r
                    assert all(value == 0 for value in dd.values())
                    counts['factor_chain_directions'] += 1
            tokens = [[None] + list(range(s['type_count'])) for s in sources]
            ab = pairs(row['pairs'][0], tokens[0], tokens[1])
            bc = pairs(row['pairs'][1], tokens[1], tokens[2])
            cd = pairs(row['pairs'][2], tokens[2], tokens[3])
            abc_l = pairs(row['triples'][0], ab.values(), tokens[2])
            abc_r = pairs(row['triples'][1], bc.values(), tokens[0])
            bcd_l = pairs(row['triples'][2], bc.values(), tokens[3])
            bcd_r = pairs(row['triples'][3], cd.values(), tokens[1])
            assert dict(row['triple_images'][0]) == {
                abc_l[(ab[(a, b)], c)]: abc_r[(bc[(b, c)], a)] for a, b, c in product(*tokens[:3])}
            assert dict(row['triple_images'][1]) == {
                bcd_l[(bc[(b, c)], d)]: bcd_r[(cd[(c, d)], b)] for b, c, d in product(*tokens[1:])}
            nested = [pairs(ts, triple.values(), tokens[f]) for ts, triple, f in
                      zip(row['nested'], [abc_l, abc_r, bcd_l, bcd_r], [3, 3, 0, 0])]
            middle = pairs(row['middle'], ab.values(), cd.values())

            def word(t):
                a, b, c, d = t
                return (nested[0][(abc_l[(ab[(a, b)], c)], d)], nested[1][(abc_r[(bc[(b, c)], a)], d)],
                        nested[2][(bcd_l[(bc[(b, c)], d)], a)], nested[3][(bcd_r[(cd[(c, d)], b)], a)],
                        middle[(ab[(a, b)], cd[(c, d)])])
            type_words = {t: word(t) for t in product(*tokens)}
            maps = []
            for e, (source, target) in enumerate(EDGES):
                mapping = dict(row['edges'][e])
                assert len(mapping) == len(row['edges'][e]) == len(set(mapping.values()))
                assert mapping == {w[source]: w[target] for w in type_words.values()}
                maps.append(mapping)
            for w in type_words.values():
                assert maps[2][maps[1][maps[0][w[0]]]] == maps[4][maps[3][w[0]]] == w[3]
                counts['complete_type_pentagons'] += 1
            expected = set(product(*(s['cells'] for s in sources)))
            grades = [0] * (sum(len(s['sizes']) - 1 for s in sources) + 1)
            for ids in expected:
                grades[sum(g for g, _ in ids)] += 1
            assert grades == row['grade_counts']
            plans[sample] = sources, expected, type_words
            seen[sample] = set()
            counts['plans'] += 1
            counts['declared_empty_grades'] += sum(n == 0 for n in grades)
            continue
        sources, expected, type_words = plans[sample]

        def paths(tree, ids):
            out = {}

            def degree(t):
                return ids[t][0] if isinstance(t, int) else degree(t[0]) + degree(t[1])

            def walk(t, path, bits):
                if isinstance(t, int):
                    out[t] = path, bits
                else:
                    walk(t[0], path + [0], bits + [0])
                    walk(t[1], path + [1], bits + [degree(t[0]) % 2])
            walk(tree, [], [])
            return out

        def flags(ids, v):
            tree = TREES[v]
            route = paths(tree, ids)
            result = []
            for f, c in enumerate(ids):
                path, bits = route[f]
                flip = sum(bits) % 2
                for lower, occurrence, pos, active, q, aspects in sources[f]['fibers'].get(c, []):
                    target = list(ids)
                    target[f] = lower
                    result.append(dict(upper=pack(tree, ids), lower=pack(tree, target), path=path, bits=bits,
                        flip=flip, factor=f, position=pos, word_position=len(result), occurrence=occurrence,
                        exists=active, factor_coefficient=str(q), coefficient=str(-q if flip else q), aspects=aspects))
            return result

        def boundary(values, v):
            result = {}
            for cell, q in values.items():
                ids = unpack(TREES[v], cell)
                assert ids in expected
                for o in flags(ids, v):
                    lower = o['lower']
                    result.setdefault(lower, Q(0))
                    if o['exists']:
                        result[lower] += q * Q(o['coefficient'])
            return result

        def image(values, source, target):
            return {pack(TREES[target], unpack(TREES[source], cell)): q for cell, q in values.items()}

        if row['phase'] == 'cell':
            cells = [freeze(c) for c in row['cells']]
            ids = unpack(TREES[0], cells[0])
            assert ids in expected and ids not in seen[sample]
            seen[sample].add(ids)
            assert cells == [pack(t, ids) for t in TREES]
            assert row['identities'] == [[c[0], s['cells'][c][0]] for c, s in zip(ids, sources)]
            assert tuple(row['types']) == type_words[tuple(s['cells'][c][1] for c, s in zip(ids, sources))]
            assert len(row['flags']) == 5
            for v in range(5):
                expected_word = flags(ids, v)
                actual_word = row['flags'][v]
                for o in actual_word:
                    o['upper'], o['lower'] = freeze(o['upper']), freeze(o['lower'])
                assert actual_word == expected_word
                counts['retained_flags'] += len(expected_word)
                counts['inactive_flags'] += sum(not o['exists'] for o in expected_word)
                counts['active_zero_flags'] += sum(o['exists'] and Q(o['coefficient']) == 0 for o in expected_word)
                counts['multiple_parity_flags'] += sum(sum(o['bits']) > 1 for o in expected_word)
                if sum(c[0] for c in ids) > 1:
                    dd = boundary(boundary({cells[v]: Q(1)}, v), v)
                    assert all(q == 0 for q in dd.values())
                    counts['nested_chain_directions'] += 1
            counts['cell_certificates'] += 1
        elif row['phase'] == 'action':
            long = [field(x) for x in row['long']]
            short = [field(x) for x in row['short']]
            assert len(long) == 4 and len(short) == 3
            assert long[0] == short[0]
            for cell in long[0]:
                ids = unpack(TREES[0], cell)
                assert ids in expected and sum(c[0] for c in ids) == row['grade']
            assert long == [image(long[0], 0, v) for v in [0, 1, 2, 3]]
            assert short == [image(long[0], 0, v) for v in [0, 4, 3]]
            assert long[-1] == short[-1]
            fields = [long[0], long[1], long[2], long[3], short[1]]
            assert len(row['edges']) == 5
            for e, (source, target) in enumerate(EDGES):
                receipt = row['edges'][e]
                assert receipt['edge'] == e
                assert field(receipt['round_trip']) == fields[source]
                if row['grade']:
                    direct = boundary(fields[target], target)
                    mapped = image(boundary(fields[source], source), source, target)
                    assert field(receipt['mapped_boundary']) == field(receipt['image_boundary']) == direct == mapped
                    counts['chain_naturality_witnesses'] += 1
                    counts['supported_zero_boundary_coordinates'] += sum(q == 0 for q in direct.values())
                else:
                    assert receipt['mapped_boundary'] is None and receipt['image_boundary'] is None
            if row['grade'] > 1:
                dd = boundary(boundary(long[0], 0), 0)
                assert field(row['second_boundary']) == dd and all(q == 0 for q in dd.values())
            else:
                assert row['second_boundary'] is None
            counts['field_pentagons'] += 1
            counts['empty_inputs'] += not long[0]
            counts['supported_zero_inputs'] += sum(q == 0 for q in long[0].values())
        else:
            raise AssertionError(row['phase'])
    for sample, (_, expected, _) in plans.items():
        assert seen[sample] == expected
    assert counts['plans'] == 12 and all(counts[key] for key in ['multiple_parity_flags', 'inactive_flags', 'active_zero_flags', 'empty_inputs'])
    print(json.dumps(dict(sorted(counts.items())), sort_keys=True))


if __name__ == '__main__':
    verify(sys.argv[1])
