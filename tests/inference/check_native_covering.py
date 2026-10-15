"""Independent original-source, covering and exact field checks using Fraction."""
from collections import Counter
from fractions import Fraction as F
import json
import subprocess
import sys

counts = Counter(arithmetic=0)


def coefficients(c, k):
    result = []
    for upper, lower, identity, exists, realized, aspects, lattice in c['occurrences'][k]:
        q = F(c['cells'][k-1][lower][2]) * int(lattice) / F(c['cells'][k][upper][2])
        assert q == F(realized)
        assert exists == aspects[0] and (exists or q == 0)
        assert not q or (q < 0) == bool(aspects[1])
        result.append(q)
    return result


def source(c):
    assert len(c['sizes']) == len(c['cells']) == len(c['occurrences'])
    for k, n in enumerate(c['sizes']):
        assert len(c['cells'][k]) == n
        assert len({row[0] for row in c['cells'][k]}) == n
        assert all(F(row[2]) > 0 for row in c['cells'][k])
        assert len({row[2] for row in c['occurrences'][k]}) == len(c['occurrences'][k])
        assert k or not c['occurrences'][k]
        if k:
            q = coefficients(c, k)
            assert len(q) == len(c['occurrences'][k])
        if k > 1:
            counts['complete_chain_families'] += 1
            for i in range(n):
                assert all(q == 0 for q in boundary(c, k-1, boundary(c, k, {i: F(1)})).values())
                counts['chain_basis_directions'] += 1


def field(raw):
    out = {i: F(q) for i, q in raw}
    assert len(out) == len(raw)
    return out


def boundary(c, k, values):
    assert k > 0
    out = {}
    coefs = coefficients(c, k)
    for slot, row in enumerate(c['occurrences'][k]):
        upper, lower, _, exists = row[:4]
        if upper in values:
            out.setdefault(lower, F(0))
            if exists:
                out[lower] += coefs[slot] * values[upper]
    return out


def strict(a, b, m, bijective=False):
    assert a['ordered_types'] == b['ordered_types']
    assert len(m['cells']) == len(m['occurrences']) == len(a['sizes']) <= len(b['sizes'])
    for k, n in enumerate(a['sizes']):
        cells, occurrences = m['cells'][k], m['occurrences'][k]
        assert len(cells) == n and all(0 <= i < b['sizes'][k] for i in cells)
        assert len(occurrences) == len(a['occurrences'][k])
        if bijective:
            assert sorted(cells) == list(range(b['sizes'][k]))
            assert sorted(occurrences) == list(range(len(b['occurrences'][k])))
        if not k:
            continue
        aq, bq = coefficients(a, k), coefficients(b, k)
        for i, j in enumerate(cells):
            assert a['cells'][k][i][1] == b['cells'][k][j][1]
            original = [p for p, row in enumerate(a['occurrences'][k]) if row[0] == i]
            target = [p for p, row in enumerate(b['occurrences'][k]) if row[0] == j]
            assert sorted(occurrences[p] for p in original) == target
            if a['ordered_types'][a['cells'][k][i][1]]:
                assert [occurrences[p] for p in original] == target
            for p in original:
                q = occurrences[p]
                x, y = a['occurrences'][k][p], b['occurrences'][k][q]
                assert m['cells'][k-1][x[1]] == y[1]
                assert x[3] == y[3] and x[5] == y[5] and aq[p] == bq[q]
                counts['complete_map_occurrences'] += 1
        counts['complete_map_cells'] += n


def covering(a, b, m):
    strict(a, b, m)
    assert len(a['sizes']) == len(b['sizes'])
    for k, n in enumerate(a['sizes']):
        assert set(m['cells'][k]) == set(range(b['sizes'][k]))
        if k+1 < len(a['sizes']):
            for i in range(n):
                actual = [m['occurrences'][k+1][p] for p, row in enumerate(a['occurrences'][k+1]) if row[1] == i]
                expected = [p for p, row in enumerate(b['occurrences'][k+1]) if row[1] == m['cells'][k][i]]
                assert sorted(actual) == expected
                counts['retained_upward_stars'] += 1


def push(m, k, values):
    out = {}
    for i, q in values.items():
        j = m['cells'][k][i]
        out[j] = out.get(j, F(0)) + q
    return out


def pull(m, k, values):
    return {i: values[j] for i, j in enumerate(m['cells'][k]) if j in values}


def walk(c, w):
    current = tuple(w['start'])
    assert 0 <= current[1] < c['sizes'][current[0]]
    visits = [list(current)]
    for k, slot, upward in w['steps']:
        row = c['occurrences'][k][slot]
        lower, upper = (k-1, row[1]), (k, row[0])
        assert current == (lower if upward else upper)
        current = upper if upward else lower
        visits.append(list(current))
    assert w['visits'] == visits
    return current


def lift(a, b, m, w, start):
    walk(b, w)
    current = tuple(start)
    assert current[0] == w['start'][0] and m['cells'][current[0]][current[1]] == w['start'][1]
    steps, visits = [], [list(current)]
    for k, target, upward in w['steps']:
        choices = []
        for p, row in enumerate(a['occurrences'][k]):
            endpoint = (k-1, row[1]) if upward else (k, row[0])
            if endpoint == current and m['occurrences'][k][p] == target:
                choices.append(p)
        assert len(choices) == 1
        p = choices[0]
        row = a['occurrences'][k][p]
        current = (k, row[0]) if upward else (k-1, row[1])
        steps.append([k, p, upward])
        visits.append(list(current))
        counts['inactive_lift_steps'] += not row[3]
        counts['active_zero_lift_steps'] += bool(row[3]) and not F(row[4])
    return {'start': list(start), 'steps': steps, 'visits': visits}


def square(r):
    e, b, p, y, t = (r[name] for name in ['E', 'B', 'P', 'Y', 'T'])
    for c in [e, b, p, y, t]:
        source(c)
    covering(e, b, r['p'])
    covering(p, y, r['q'])
    strict(p, e, r['into'])
    strict(y, b, r['base_map'])
    strict(t, e, r['cone_into'])
    strict(t, y, r['cone_base'])
    strict(t, p, r['mediator'])
    for kind, sizes in [('cells', [len(row) for row in e['cells']]), ('occurrences', [len(row) for row in e['occurrences']])]:
        for k, n in enumerate(sizes):
            pairs = {(a, z) for a in range(n) for z in range(len(y[kind][k])) if r['p'][kind][k][a] == r['base_map'][kind][k][z]}
            actual = {(a, z): i for i, (a, z) in enumerate(zip(r['into'][kind][k], r['q'][kind][k]))}
            assert len(actual) == len(p[kind][k]) and set(actual) == pairs
            for i in range(len(t[kind][k])):
                assert r['mediator'][kind][k][i] == actual[(r['cone_into'][kind][k][i], r['cone_base'][kind][k][i])]
                counts['mediator_directions'] += 1
            counts['compatible_' + kind] += len(pairs)
    counts['pullback_squares'] += 1


def main():
    run = subprocess.run([sys.argv[1], '--json'], check=True, text=True, capture_output=True)
    assert not run.stderr
    cases = {}
    for line in run.stdout.splitlines():
        r = json.loads(line)
        kind = r['kind']
        if kind == 'covering':
            assert r['case'] not in cases
            cases[r['case']] = r
            source(r['cover']); source(r['base'])
            covering(r['cover'], r['base'], r['projection'])
            counts['coverings'] += 1
        elif kind == 'square':
            square(r)
        else:
            c = cases[r['case']]
            a, b, m = c['cover'], c['base'], c['projection']
            if kind == 'field':
                k, x, out, action = r['grade'], field(r['input']), field(r['output']), r['action']
                if action in ['transfer', 'pullback']:
                    assert out == pull(m, k, x)
                    if action == 'transfer' and k:
                        assert boundary(a, k, out) == pull(m, k-1, boundary(b, k, x))
                elif action == 'pushforward':
                    assert out == push(m, k, x)
                    if k:
                        assert boundary(b, k, out) == push(m, k-1, boundary(a, k, x))
                else:
                    assert action == 'descent'
                    expected = {}
                    for j in set(m['cells'][k]):
                        fiber = [i for i, v in enumerate(m['cells'][k]) if v == j]
                        present = [i for i in fiber if i in x]
                        assert not present or len(present) == len(fiber)
                        if present:
                            assert len({x[i] for i in present}) == 1
                            expected[j] = x[present[0]]
                    assert out == expected
                counts['field_cases'] += 1
                counts['supported_zero_outputs'] += sum(q == 0 for q in out.values())
            elif kind == 'lift':
                assert r['lifted_walk'] == lift(a, b, m, r['base_walk'], r['lifted_walk']['start'])
                walk(a, r['lifted_walk'])
                counts['lift_cases'] += 1
            elif kind == 'monodromy':
                w = r['loop']
                assert walk(b, w) == tuple(w['start'])
                k, j = w['start']
                fiber = [i for i, v in enumerate(m['cells'][k]) if v == j]
                assert r['fiber'] == fiber
                image = [fiber.index(lift(a, b, m, w, [k, i])['visits'][-1][1]) for i in fiber]
                assert image == r['image'] and sorted(image) == list(range(len(fiber)))
                counts['monodromy_cases'] += 1
                counts['nontrivial_permutations'] += image != list(range(len(fiber)))
            else:
                assert kind == 'deck'
                strict(a, a, r['forward'], True); strict(a, a, r['inverse'], True)
                for family in ['cells', 'occurrences']:
                    for k, row in enumerate(r['forward'][family]):
                        for i, j in enumerate(row):
                            assert r['inverse'][family][k][j] == i and m[family][k][i] == m[family][k][j]
                counts['deck_cases'] += 1
    assert counts['coverings'] == 8 and counts['pullback_squares'] == 9
    assert counts['inactive_lift_steps'] and counts['active_zero_lift_steps']
    assert counts['nontrivial_permutations'] == 3
    result = dict(sorted(counts.items()))
    result['arithmetic'] = 'fractions.Fraction'
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
