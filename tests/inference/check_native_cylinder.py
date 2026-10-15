#!/usr/bin/env python3
"""Independent Fraction model of retained cylinder routes, chain identities and maps."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def values(rows, product=False):
    out = {}
    for cell, q in rows:
        key = tuple(cell) if product else cell
        assert key not in out
        out[key] = Q(q)
    return out


def factor_boundary(source, grade, field):
    assert grade > 0
    out = {}
    for cell, q in field.items():
        for upper, lower, _, active, coefficient, *_ in source['occurrences'][grade]:
            if upper == cell:
                out.setdefault(lower, Q(0))
                if active:
                    out[lower] += q * Q(coefficient)
    return out


def factor(source):
    assert len(source['sizes']) == len(source['cells']) == len(source['occurrences'])
    families = 0
    for k, size in enumerate(source['sizes']):
        assert len(source['cells'][k]) == size
        assert len({c[0] for c in source['cells'][k]}) == size
        assert all(Q(c[2]) > 0 for c in source['cells'][k])
        for upper, lower, _, _, coefficient, _, lattice in source['occurrences'][k]:
            assert k and 0 <= upper < size and 0 <= lower < source['sizes'][k - 1]
            assert Q(coefficient) == Q(source['cells'][k - 1][lower][2]) * Q(lattice) / Q(source['cells'][k][upper][2])
        if k > 1:
            for i in range(size):
                d = factor_boundary(source, k, {i: Q(1)})
                assert all(q == 0 for q in factor_boundary(source, k - 1, d).values())
                families += 1
    return families


def interval(source):
    factor(source)
    assert source['sizes'] == [2, 1]
    assert source['cells'][0] == [[0, None, '1'], [1, None, '1']]
    assert factor_boundary(source, 1, {0: Q(1)}) == {0: Q(-1), 1: Q(1)}
    assert len(source['occurrences'][1]) == 2


def routes(source, right, id):
    p, a, q, b = id
    result = []
    for side, c, k, upper in [(0, source, p, a), (1, right, q, b)]:
        if not k:
            continue
        position = 0
        for u, lower, key, active, coefficient, aspects, _ in c['occurrences'][k]:
            if u != upper:
                continue
            target = (p, a, q - 1, lower) if side else (p - 1, lower, q, b)
            flip = bool(side and p % 2)
            value = -Q(coefficient) if flip else Q(coefficient)
            result.append([list(target), side, key, position, flip, bool(active), Q(coefficient), value, aspects])
            position += 1
    return result


def boundary(source, right, field):
    out = {}
    for id, q in field.items():
        assert id[0] + id[2] > 0
        for lower, _, _, _, _, active, _, coefficient, _ in routes(source, right, id):
            key = tuple(lower)
            out.setdefault(key, Q(0))
            if active:
                out[key] += q * coefficient
    return out


def endpoint(k, field, end):
    return {(k, i, 0, end): q for i, q in field.items()}


def homotopy(k, field):
    return {(k, i, 1, 0): (-q if k % 2 else q) for i, q in field.items()}


def combine(a, b, sign=1):
    out = dict(a)
    for i, q in b.items():
        out.setdefault(i, Q(0))
        out[i] += sign * q
    return out


def complete_map(source, target, declaration):
    cells, occurrences = declaration['cells'], declaration['occurrences']
    assert len(cells) == len(occurrences) == len(source['sizes'])
    assert len(target['sizes']) >= len(source['sizes'])
    assert source['ordered_types'] == target['ordered_types']
    directions = 0
    for k, size in enumerate(source['sizes']):
        assert len(cells[k]) == size and len(occurrences[k]) == len(source['occurrences'][k])
        assert all(0 <= i < target['sizes'][k] for i in cells[k])
        for i in range(size):
            assert source['cells'][k][i][1] == target['cells'][k][cells[k][i]][1]
            fiber = [p for p, o in enumerate(source['occurrences'][k]) if o[0] == i]
            image = [p for p, o in enumerate(target['occurrences'][k]) if o[0] == cells[k][i]]
            mapped = [occurrences[k][p] for p in fiber]
            assert sorted(mapped) == image
            t = source['cells'][k][i][1]
            if t is not None and source['ordered_types'][t]:
                assert mapped == image
        for p, (upper, lower, _, active, coefficient, aspects, _) in enumerate(source['occurrences'][k]):
            u, l, _, a, q, bits, _ = target['occurrences'][k][occurrences[k][p]]
            assert (u, l) == (cells[k][upper], cells[k - 1][lower])
            assert (active, Q(coefficient), aspects) == (a, Q(q), bits)
        directions += size + len(occurrences[k])
    return directions


def apply(field, left, right):
    out = {}
    for (p, i, q, j), value in field.items():
        target = (p, left['cells'][p][i], q, right['cells'][q][j])
        out.setdefault(target, Q(0))
        out[target] += value
    return out


def main():
    run = subprocess.run([sys.argv[1], '--json'], check=True, capture_output=True, text=True)
    assert not run.stderr
    stats = dict(source_chain_cases=0, cylinder_map_cases=0, prism_route_cases=0,
                 retained_routes=0, inactive_routes=0, active_zero_routes=0,
                 odd_parity_routes=0, supported_zero_inputs=0, supported_zero_residuals=0,
                 complete_map_directions=0, noninjective_cases=0, padded_target_cases=0,
                 factor_chain_families=0, arithmetic='fractions.Fraction')
    for line in run.stdout.splitlines():
        r = json.loads(line)
        source, right = r['source'], r['interval']
        stats['factor_chain_families'] += factor(source)
        interval(right)
        k = r['grade']
        if r['kind'] == 'routes':
            expected = routes(source, right, (k, r['cell'], 1, 0))
            actual = r['routes']
            for row in actual:
                row[6], row[7] = Q(row[6]), Q(row[7])
            assert actual == expected
            stats['prism_route_cases'] += 1
            stats['retained_routes'] += len(expected)
            stats['inactive_routes'] += sum(not row[5] for row in expected)
            stats['active_zero_routes'] += sum(row[5] and row[6] == 0 for row in expected)
            stats['odd_parity_routes'] += sum(row[4] for row in expected)
        elif r['kind'] == 'witness':
            field = values(r['input'])
            start, finish, h = endpoint(k, field, 0), endpoint(k, field, 1), homotopy(k, field)
            d = factor_boundary(source, k, field) if k else None
            assert d == (None if r['source_boundary'] is None else values(r['source_boundary']))
            dh, hd = boundary(source, right, h), homotopy(k - 1, d) if k else {}
            difference = combine(finish, start, -1)
            residual = combine(difference, combine(dh, hd), -1)
            for name, expected in [('start', start), ('finish', finish), ('homotopy', h), ('dH', dh), ('Hd', hd), ('difference', difference), ('residual', residual)]:
                assert values(r[name], True) == expected, name
            assert all(q == 0 for q in residual.values())
            if k:
                assert boundary(source, right, start) == endpoint(k - 1, d, 0)
                assert boundary(source, right, finish) == endpoint(k - 1, d, 1)
                assert all(q == 0 for q in boundary(source, right, dh).values())
            stats['source_chain_cases'] += 1
            stats['supported_zero_inputs'] += sum(q == 0 for q in field.values())
            stats['supported_zero_residuals'] += len(residual)
        else:
            assert r['kind'] == 'map'
            target, tright = r['target'], r['target_interval']
            stats['factor_chain_families'] += factor(target)
            interval(tright)
            left, right_map = r['source_map'], r['interval_map']
            stats['complete_map_directions'] += complete_map(source, target, left) + complete_map(right, tright, right_map)
            assert right_map['cells'] == [[0, 1], [0]] and right_map['occurrences'] == [[], [0, 1]]
            field, output = values(r['input'], True), values(r['output'], True)
            assert all(id[0] + id[2] == k for id in field)
            assert apply(field, left, right_map) == output
            if k:
                before, after = boundary(source, right, field), boundary(target, tright, output)
                assert before == values(r['boundary'], True) and after == values(r['target_boundary'], True)
                assert apply(before, left, right_map) == after
            else:
                assert r['boundary'] is r['target_boundary'] is None
            stats['cylinder_map_cases'] += 1
            stats['noninjective_cases'] += any(len(set(row)) < len(row) for row in left['cells'])
            stats['padded_target_cases'] += len(target['sizes']) > len(source['sizes'])
    assert stats['noninjective_cases'] and stats['padded_target_cases'] and stats['inactive_routes'] and stats['active_zero_routes']
    print(json.dumps(stats, sort_keys=True))


if __name__ == '__main__':
    main()
