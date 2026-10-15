"""Check both owned products and graded interchange with fractions.Fraction."""
from collections import Counter
from fractions import Fraction as Q
from itertools import product
import json
import subprocess
import sys


def frozen(value):
    return tuple(frozen(x) for x in value) if isinstance(value, list) else value


def field(value):
    out = {frozen(c): Q(q) for c, q in value}
    assert len(out) == len(value), 'repeated field coordinates'
    return out


def swapped(values, signed=True):
    return {(b, a): (-q if signed and a[0] % 2 and b[0] % 2 else q)
            for (a, b), q in values.items()}


def verify(exe):
    done = subprocess.run([exe, '--records'], text=True, capture_output=True, check=True)
    assert not done.stderr
    counts = Counter(arithmetic='fractions.Fraction')
    plans, seen, actions = {}, {}, set()
    for line in done.stdout.splitlines():
        row = json.loads(line)
        sample = row['sample']
        if row['phase'] == 'plan':
            assert sample not in plans
            sources = row['sources']
            for source in sources:
                original = source['cells']
                source['cells'] = {frozen(c): (key, t, Q(scale)) for c, key, t, scale in original}
                assert len(original) == len(source['cells'])
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
            tokens = [[None] + list(range(s['type_count'])) for s in sources]
            ab, ba = [{(a, b): key for a, b, key in ts} for ts in row['products']]
            for table, ts in zip([ab, ba], row['products']):
                assert len(table) == len(ts) == len(set(table.values()))
            assert set(ab) == set(product(*tokens)) and set(ba) == set(product(*reversed(tokens)))
            images = {a: b for a, b in row['types']}
            assert len(images) == len(row['types']) == len(set(images.values()))
            assert images == {ab[(a, b)]: ba[(b, a)] for a, b in product(*tokens)}
            cells = set(product(*(s['cells'] for s in sources)))
            grade_counts = [0] * (sum(len(s['sizes']) - 1 for s in sources) + 1)
            for a, b in cells:
                grade_counts[a[0] + b[0]] += 1
            assert row['grade_counts'] == grade_counts
            plans[sample] = sources, [ab, ba], cells, grade_counts
            seen[sample] = set()
            counts['plans'] += 1
            counts['complete_type_pairs'] += len(images)
            counts['declared_empty_grades'] += sum(n == 0 for n in grade_counts)
            continue
        sources, types, cells, grade_counts = plans[sample]
        order = row['order']
        assert order in [0, 1]

        def flags(c, order):
            out = []
            for side, id_ in enumerate(c):
                for l, occurrence, pos, exists, value, aspects in sources[side ^ order]['fibers'].get(id_, []):
                    lower = list(c)
                    lower[side] = l
                    flip = int(side == 1 and c[0][0] % 2 == 1)
                    out.append(dict(upper=c, lower=tuple(lower), side=side, position=pos, occurrence=occurrence,
                        flip=flip, exists=exists, factor_coefficient=str(value),
                        coefficient=str(-value if flip else value), aspects=aspects))
            return out

        def boundary(values, order):
            out = {}
            for c, value in values.items():
                assert (c if order == 0 else tuple(reversed(c))) in cells
                for flag in flags(c, order):
                    l = flag['lower']
                    out.setdefault(l, Q(0))
                    if flag['exists']:
                        out[l] += value * Q(flag['coefficient'])
            return out

        if row['phase'] == 'cell':
            c, t = frozen(row['source']), frozen(row['target'])
            ids = c if order == 0 else tuple(reversed(c))
            assert ids in cells and (order, ids) not in seen[sample]
            seen[sample].add((order, ids))
            assert t == tuple(reversed(c))
            identities = [[id_[0], sources[f]['cells'][id_][0]] for f, id_ in enumerate(ids)]
            assert row['identities'] == ([identities, list(reversed(identities))] if order == 0 else [list(reversed(identities)), identities])
            ts = [sources[f]['cells'][id_][1] for f, id_ in enumerate(ids)]
            source_ts = tuple(ts if order == 0 else reversed(ts))
            assert row['types'] == [types[order][source_ts], types[1-order][tuple(reversed(source_ts))]]
            upper_flip = int(c[0][0] % 2 and c[1][0] % 2)
            assert row['flip'] == upper_flip
            a, b = flags(c, order), flags(t, 1-order)
            for word, expected in zip([row['source_flags'], row['target_flags']], [a, b]):
                for flag in word:
                    flag['upper'], flag['lower'] = frozen(flag['upper']), frozen(flag['lower'])
                assert word == expected
            target_positions = {(flag['side'] ^ (1-order), flag['position']): i for i, flag in enumerate(b)}
            expected = []
            for i, flag in enumerate(a):
                j = target_positions[(flag['side'] ^ order, flag['position'])]
                image = b[j]
                lower_flip = int(flag['lower'][0][0] % 2 and flag['lower'][1][0] % 2)
                mapped = (-1 if lower_flip else 1) * Q(flag['coefficient'])
                direct = (-1 if upper_flip else 1) * Q(image['coefficient'])
                assert mapped == direct
                assert flag['exists'] == image['exists'] and flag['aspects'] == image['aspects']
                assert flag['occurrence'] == image['occurrence'] and flag['factor_coefficient'] == image['factor_coefficient']
                expected.append([i, j, upper_flip, lower_flip, str(mapped), str(direct)])
                counts['retained_flag_pairs'] += 1
                counts['inactive_flag_pairs'] += not flag['exists']
                counts['active_zero_flag_pairs'] += flag['exists'] and Q(flag['coefficient']) == 0
                counts['reordered_flags'] += i != j
                counts['odd_interchange_flags'] += upper_flip
            assert row['correspondence'] == expected
            assert sorted(x[1] for x in expected) == list(range(len(b)))
            counts['cell_certificates'] += 1
        else:
            assert row['phase'] == 'action'
            g = row['grade']
            values = field(row['input'])
            assert g < len(grade_counts)
            for c in values:
                assert (c if order == 0 else tuple(reversed(c))) in cells and c[0][0] + c[1][0] == g
            image = swapped(values)
            assert field(row['image']) == image and field(row['round_trip']) == values == swapped(image)
            counts['empty_inputs'] += not values
            counts['supported_zero_inputs'] += sum(q == 0 for q in values.values())
            if g:
                before, direct = boundary(values, order), boundary(image, 1-order)
                assert direct == swapped(before) == field(row['mapped_boundary']) == field(row['image_boundary'])
                counts['chain_naturality_witnesses'] += 1
                counts['supported_zero_boundary_coordinates'] += sum(q == 0 for q in direct.values())
                counts['unsigned_swap_counterexamples'] += swapped(before, False) != boundary(swapped(values, False), 1-order)
                if g > 1:
                    dd = boundary(before, order)
                    assert dd == field(row['dd']) and all(q == 0 for q in dd.values())
                    counts['product_chain_directions'] += 1
                else:
                    assert row['dd'] is None
            else:
                assert row['mapped_boundary'] is row['image_boundary'] is row['dd'] is None
            actions.add((sample, order, g))
            counts['field_witnesses'] += 1
    assert set(plans) == set(range(16))
    for sample, (_, _, cells, grades) in plans.items():
        assert seen[sample] == set(product([0, 1], cells))
        assert all((sample, order, g) in actions for order, g in product([0, 1], range(len(grades))))
    assert all(counts[k] > 0 for k in ['inactive_flag_pairs', 'reordered_flags', 'odd_interchange_flags',
        'active_zero_flag_pairs', 'declared_empty_grades', 'supported_zero_boundary_coordinates', 'unsigned_swap_counterexamples'])
    return dict(sorted(counts.items()))


if __name__ == '__main__':
    print(json.dumps(verify(sys.argv[1]), sort_keys=True))
