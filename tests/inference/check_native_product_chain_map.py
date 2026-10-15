"""Independent Fraction reconstruction of retained three-factor map witnesses."""
from collections import Counter
from fractions import Fraction as Q
from itertools import permutations
import json
import subprocess
import sys
import tempfile

ORDERS = list(permutations(range(3)))
counts = Counter()


def ids(value):
    return tuple(tuple(c) for c in value)


def values(raw):
    result = {ids(c): Q(q) for c, q in raw}
    assert len(result) == len(raw)
    return result


def field(raw):
    c, grade = raw['carrier'], raw['grade']
    assert 0 <= c < 12
    order = ORDERS[c // 2]
    result = {}
    for local, q in values(raw['values']).items():
        original = [None] * 3
        for j, f in enumerate(order):
            original[f] = local[j]
        assert sum(g for g, _ in original) == grade
        result[tuple(original)] = q
    return c, grade, result


def factor_boundary(c, k):
    rows = c['sizes'][k-1] if k-1 < len(c['sizes']) else 0
    cols = c['sizes'][k] if k < len(c['sizes']) else 0
    result = [[Q(0) for _ in range(cols)] for _ in range(rows)]
    for g, upper, lower, active, lattice in c['flags']:
        if g == k and active:
            result[lower][upper] += Q(lattice) * Q(c['scales'][k-1][lower]) / Q(c['scales'][k][upper])
    return result


def matrix(raw):
    rows, cols = raw['shape']
    result = [[Q(0) for _ in range(cols)] for _ in range(rows)]
    seen = set()
    for i, j, value in raw['support']:
        assert (i, j) not in seen and 0 <= i < rows and 0 <= j < cols
        seen.add((i, j))
        result[i][j] = Q(value)
        counts['declared_zero_map_directions' if Q(value) == 0 else 'nonzero_map_directions'] += 1
    return result


def product(a, b, rows, cols, middle):
    return [[sum((a[i][k] * b[k][j] for k in range(middle)), Q(0)) for j in range(cols)] for i in range(rows)]


def check_plan(p):
    for side in ['source', 'target']:
        for c in p[side]:
            assert len(c['sizes']) == len(c['scales'])
            assert all(len(s) == n for s, n in zip(c['scales'], c['sizes']))
            counts['declared_empty_grades'] += c['sizes'].count(0)
            counts['retained_factor_flags'] += len(c['flags'])
            for k in range(2, len(c['sizes'])):
                b, a = factor_boundary(c, k), factor_boundary(c, k-1)
                assert all(v == 0 for row in product(a, b, c['sizes'][k-2], c['sizes'][k], c['sizes'][k-1]) for v in row)
                counts['strict_factor_chain_directions'] += c['sizes'][k]
    for f, maps in enumerate(p['maps']):
        a, b = p['source'][f], p['target'][f]
        assert len(maps) == len(b['sizes']) >= len(a['sizes'])
        matrices = [matrix(m) for m in maps]
        for k, m in enumerate(maps):
            assert m['shape'] == [b['sizes'][k], a['sizes'][k] if k < len(a['sizes']) else 0]
            if k:
                source_upper = a['sizes'][k] if k < len(a['sizes']) else 0
                source_lower = a['sizes'][k-1] if k-1 < len(a['sizes']) else 0
                lhs = product(factor_boundary(b, k), matrices[k], b['sizes'][k-1], source_upper, b['sizes'][k])
                rhs = product(matrices[k-1], factor_boundary(a, k), b['sizes'][k-1], source_upper, source_lower)
                assert lhs == rhs
                counts['complete_factor_chain_identities'] += 1
    counts['plans'] += 1


def apply_map(p, x):
    c, grade, current = x
    stages = []
    for f in range(3):
        image, contributions = {}, []
        for original, value in sorted(current.items()):
            g, column = original[f]
            for row, col, coefficient in sorted(p['maps'][f][g]['support']):
                if col != column:
                    continue
                target = list(original)
                target[f] = (g, row)
                target = tuple(target)
                q = Q(coefficient)
                contributions.append((original, target, (row, col), value, q, value*q))
                image[target] = image.get(target, Q(0)) + value*q
        stages.append((current, image, contributions))
        current = image
    return (c, grade, current), stages


def check_lift(p, raw, expected_input):
    actual_input = field(raw['input'])
    assert actual_input == expected_input
    expected, stages = apply_map(p, actual_input)
    assert field(raw['image']) == expected
    assert len(raw['stages']) == 3
    for f, (s, (before, after, contributions)) in enumerate(zip(raw['stages'], stages)):
        assert s['factor'] == f
        assert s['input_target'] == [int(j < f) for j in range(3)]
        assert s['image_target'] == [int(j <= f) for j in range(3)]
        assert values(s['input']) == before and values(s['image']) == after
        actual = [(ids(a), ids(b), tuple(position), Q(value), Q(coefficient), Q(result))
                  for a, b, position, value, coefficient, result in s['contributions']]
        assert actual == contributions
        counts['retained_contributions'] += len(actual)
        counts['zero_contributions'] += sum(t[-1] == 0 for t in actual)
        counts['stages'] += 1
    counts['prefix_lifts'] += 1
    return expected


def step(x, action):
    c, grade, v = x
    order = list(ORDERS[c//2])
    right = c % 2
    image = {}
    for original, q in v.items():
        p, r, s = [original[f][0] for f in order]
        parity = 0 if action == 0 else (r*s if right else p*r) if action == 1 else p*(r+s) if right else (p+r)*s
        image[original] = -q if parity % 2 else q
        if parity % 2 and q:
            counts['unsigned_interchange_counterexamples'] += 1
    if action == 1:
        i = 1 if right else 0
        order[i], order[i+1] = order[i+1], order[i]
    elif action == 2:
        i = 1 if right else 2
        order = order[i:]+order[:i]
    assert action in (0, 1, 2)
    return 2*ORDERS.index(tuple(order))+(right if action == 1 else 1-right), grade, image


def boundary(factors, x):
    c, grade, v = x
    assert grade > 0
    out = {}
    order = ORDERS[c//2]
    for original, value in v.items():
        for local, f in enumerate(order):
            g, upper = original[f]
            if not g:
                continue
            parity = sum(original[j][0] for j in order[:local]) % 2
            factor = factors[f]
            for k, u, lower, active, lattice in factor['flags']:
                if k != g or u != upper:
                    continue
                image = list(original)
                image[f] = (g-1, lower)
                image = tuple(image)
                out.setdefault(image, Q(0))
                if active:
                    q = Q(lattice)*Q(factor['scales'][g-1][lower])/Q(factor['scales'][g][upper])
                    out[image] += value*q*(-1 if parity else 1)
    return c, grade-1, out


def check_comparison(raw, first, second):
    assert field(raw['first']) == first and field(raw['second']) == second
    assert first[:2] == second[:2]
    a, b = first[2], second[2]
    assert all(a.get(k, Q(0)) == b.get(k, Q(0)) for k in a.keys() | b.keys())
    order = ORDERS[first[0]//2]
    def original(local):
        out = [None]*3
        for i, f in enumerate(order):
            out[f] = tuple(local[i])
        return tuple(out)
    assert {original(x) for x in raw['only_first']} == a.keys()-b.keys()
    assert {original(x) for x in raw['only_second']} == b.keys()-a.keys()
    assert all(a.get(k, b.get(k)) == 0 for k in a.keys() ^ b.keys())
    counts['supported_zero_differences'] += len(a.keys() ^ b.keys())
    counts['exact_naturality_comparisons'] += 1


def word(c, route):
    out = []
    for generator in ([0, 1, 0] if route == 0 else [1, 0, 1]):
        actions = [1] if (generator == 0) == (c % 2 == 0) else [0, 1, 0]
        for a in actions:
            out.append(a)
            c = step((c, 0, {}), a)[0]
    return out


def check_witness(p, w):
    x = field(w['input'])
    mapped, _ = apply_map(p, x)
    assert len(w['routes']) == 5
    expected_operations = [[0], [1], [2], word(x[0], 0), word(x[0], 1)]
    ends = []
    for route, operations in zip(w['routes'], expected_operations):
        assert route['operations'] == operations
        assert len(route['source']) == len(route['target']) == len(route['maps']) == len(operations)+1
        assert len(route['comparisons']) == len(operations)
        source, target = x, mapped
        assert field(route['source'][0]) == source and field(route['target'][0]) == target
        check_lift(p, route['maps'][0], source)
        for i, operation in enumerate(operations):
            source, target = step(source, operation), step(target, operation)
            assert field(route['source'][i+1]) == source and field(route['target'][i+1]) == target
            prefix = check_lift(p, route['maps'][i+1], source)
            check_comparison(route['comparisons'][i], target, prefix)
            counts['primitive_steps'] += 1
        ends.append(target)
    check_comparison(w['yang_baxter'], ends[3], ends[4])
    if x[1]:
        b = w['boundary']
        check_lift(p, b['input_map'], x)
        source_boundary = boundary(p['source'], x)
        image_boundary = boundary(p['target'], mapped)
        mapped_boundary = check_lift(p, b['boundary_map'], source_boundary)
        check_comparison(b['comparison'], image_boundary, mapped_boundary)
        counts['boundary_identities'] += 1
    else:
        assert w['boundary'] is None
        counts['grade_zero_without_boundary'] += 1
    counts['empty_inputs'] += not x[2]
    counts['supported_zero_inputs'] += sum(q == 0 for q in x[2].values())
    counts['witnesses'] += 1


def main():
    plans = {}
    with tempfile.TemporaryFile(mode='w+') as err:
        process = subprocess.Popen([sys.argv[1], '--emit'], stdout=subprocess.PIPE, stderr=err, text=True)
        try:
            for line in process.stdout:
                record = json.loads(line)
                if record['phase'] == 'plan':
                    check_plan(record)
                    assert record['sample'] not in plans
                    plans[record['sample']] = record
                else:
                    assert record['phase'] == 'witness'
                    check_witness(plans[record['sample']], record)
            assert process.wait() == 0
            err.seek(0)
            assert not err.read()
        finally:
            process.stdout.close()
            if process.poll() is None:
                process.kill()
                process.wait()
    assert len(plans) == 6 and counts['witnesses'] == 300
    assert counts['supported_zero_differences'] and counts['declared_zero_map_directions']
    print(json.dumps(dict(arithmetic='fractions.Fraction', **dict(sorted(counts.items()))), sort_keys=True))


if __name__ == '__main__':
    main()
