"""Reconstruct nested carriers and uncontracted flags with fractions.Fraction."""
from collections import Counter
from fractions import Fraction as Q
from itertools import product
import json
import subprocess
import sys


def frozen(value):
    return tuple(frozen(x) for x in value) if isinstance(value, list) else value


def unpack(cell, side):
    return (cell[0][0], cell[0][1], cell[1]) if side == 0 else (cell[0], cell[1][0], cell[1][1])


def pack(ids, side):
    a, b, c = ids
    return ((a, b), c) if side == 0 else (a, (b, c))


def field(value):
    out = {frozen(c): Q(q) for c, q in value}
    assert len(out) == len(value), 'repeated field coordinates'
    return out


def verify(exe):
    done = subprocess.run([exe, '--records'], text=True, capture_output=True, check=True)
    assert not done.stderr
    counts = Counter(arithmetic='fractions.Fraction')
    plans = {}
    seen = {}
    for line in done.stdout.splitlines():
        row = json.loads(line)
        sample = row['sample']
        if row['phase'] == 'plan':
            assert sample not in plans
            sources = row['sources']
            for source in sources:
                source['cells'] = {frozen(c): (key, t, Q(scale)) for c, key, t, scale in source['cells']}
                fibers = {}
                for g, upper, lower, occurrence, pos, exists, lattice, aspects in source['flags']:
                    u, l = (g, upper), (g - 1, lower)
                    assert u in source['cells'] and l in source['cells']
                    assert aspects[0] == exists
                    q = source['cells'][l][2] * Q(lattice) / source['cells'][u][2]
                    word = fibers.setdefault(u, [])
                    assert pos == len(word)
                    word.append((l, occurrence, pos, exists, q, aspects))
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
            tokens = [[None] + list(range(s['type_count'])) for s in sources]
            ab, bc = [{(a, b): key for a, b, key in ts} for ts in row['inner_types']]
            assert set(ab) == set(product(tokens[0], tokens[1]))
            assert set(bc) == set(product(tokens[1], tokens[2]))
            left, right = [{(a, b): key for a, b, key in ts} for ts in row['outer_types']]
            assert set(left) == set(product(ab.values(), tokens[2]))
            assert set(right) == set(product(bc.values(), tokens[0]))
            type_images = {a: b for a, b in row['types']}
            assert len(type_images) == len(row['types']) == len(set(type_images.values()))
            assert type_images == {left[(ab[(a, b)], c)]: right[(bc[(b, c)], a)] for a, b, c in product(*tokens)}
            expected_cells = set(product(*(s['cells'] for s in sources)))
            grade_counts = [0] * (sum(len(s['sizes']) - 1 for s in sources) + 1)
            for ids in expected_cells:
                grade_counts[sum(g for g, _ in ids)] += 1
            assert row['grade_counts'] == grade_counts
            plans[sample] = sources, ab, bc, left, right, expected_cells
            seen[sample] = set()
            counts['plans'] += 1
            counts['complete_type_triples'] += len(type_images)
            counts['declared_empty_grades'] += sum(n == 0 for n in grade_counts)
            continue
        sources, ab, bc, left, right, expected_cells = plans[sample]

        def flags(ids, side):
            upper = pack(ids, side)
            out = []
            p, q, _ = [g for g, _ in ids]
            for f, id_ in enumerate(ids):
                for l, occurrence, pos, exists, value, aspects in sources[f]['fibers'].get(id_, []):
                    lower = list(ids)
                    lower[f] = l
                    if side == 0:
                        inner_flip = int(f == 1 and p % 2 == 1)
                        outer_flip = int(f == 2 and (p + q) % 2 == 1)
                        path = [[0, 0], [0, 1], [1]][f]
                    else:
                        inner_flip = int(f == 2 and q % 2 == 1)
                        outer_flip = int(f > 0 and p % 2 == 1)
                        path = [[0], [1, 0], [1, 1]][f]
                    flip = inner_flip ^ outer_flip
                    out.append(dict(upper=upper, lower=pack(lower, side), path=path, factor=f,
                        position=pos, word_position=len(out), occurrence=occurrence,
                        flips=[inner_flip, outer_flip, flip], exists=exists,
                        coefficient=str(-value if flip else value), factor_coefficient=str(value), aspects=aspects))
            return out

        def boundary(values, side):
            out = {}
            for c, value in values.items():
                ids = unpack(c, side)
                assert ids in expected_cells
                for o in flags(ids, side):
                    l = o['lower']
                    out.setdefault(l, Q(0))
                    if o['exists']:
                        out[l] += value * Q(o['coefficient'])
            return out

        if row['phase'] == 'cell':
            lc, rc = frozen(row['left']), frozen(row['right'])
            ids = unpack(lc, 0)
            assert ids in expected_cells and ids not in seen[sample]
            seen[sample].add(ids)
            assert rc == pack(ids, 1)
            types = [s['cells'][c][1] for s, c in zip(sources, ids)]
            a, b, c = types
            assert row['types'] == [left[(ab[(a, b)], c)], right[(bc[(b, c)], a)]]
            identities = [[id_[0], source['cells'][id_][0]] for source, id_ in zip(sources, ids)]
            a, b, c = identities
            assert row['identities'] == [[a, b], c, a, [b, c]]
            lflags, rflags = flags(ids, 0), flags(ids, 1)
            pairs = row['flags']
            assert len(lflags) == len(rflags) == len(pairs)
            for pair, x, y in zip(pairs, lflags, rflags):
                for actual, expected in [(pair[0], x), (pair[1], y)]:
                    actual['upper'] = frozen(actual['upper'])
                    actual['lower'] = frozen(actual['lower'])
                    assert actual == expected
                assert x['coefficient'] == y['coefficient']
                counts['retained_flag_pairs'] += 1
                counts['inactive_flag_pairs'] += not x['exists']
                counts['active_zero_flag_pairs'] += x['exists'] and Q(x['coefficient']) == 0
                counts['double_parity_flags'] += y['flips'][0] and y['flips'][1]
            counts['cell_certificates'] += 1
        else:
            assert row['phase'] == 'action'
            side, g = row['side'], row['grade']
            value = field(row['input'])
            for c in value:
                assert unpack(c, side) in expected_cells
                assert sum(d for d, _ in unpack(c, side)) == g
            mapped = {pack(unpack(c, side), 1 - side): q for c, q in value.items()}
            assert field(row['image']) == mapped
            assert field(row['round_trip']) == value
            counts['empty_inputs'] += not value
            counts['supported_zero_inputs'] += sum(q == 0 for q in value.values())
            if g:
                direct = boundary(mapped, 1 - side)
                before = boundary(value, side)
                transported = {pack(unpack(c, side), 1 - side): q for c, q in before.items()}
                assert direct == transported == field(row['boundary']) == field(row['mapped_boundary'])
                counts['chain_naturality_witnesses'] += 1
                counts['supported_zero_boundary_coordinates'] += sum(q == 0 for q in direct.values())
                if g > 1:
                    dd = boundary(before, side)
                    assert dd == field(row['second_boundary'])
                    assert all(q == 0 for q in dd.values())
                    counts['nested_chain_directions'] += 1
                else:
                    assert row['second_boundary'] is None
            else:
                assert row['boundary'] is row['mapped_boundary'] is row['second_boundary'] is None
            counts['field_witnesses'] += 1
    assert set(plans) == set(range(12))
    for sample, plan in plans.items():
        assert seen[sample] == plan[-1]
    assert counts['inactive_flag_pairs'] > 0 and counts['double_parity_flags'] > 0
    assert counts['declared_empty_grades'] > 0 and counts['supported_zero_boundary_coordinates'] > 0
    return dict(sorted(counts.items()))


if __name__ == '__main__':
    print(json.dumps(verify(sys.argv[1]), sort_keys=True))
