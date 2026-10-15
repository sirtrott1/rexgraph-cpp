"""Independent exact reconstruction from retained factors and complete maps."""
from fractions import Fraction as Q
import itertools
import json
import subprocess
import sys


def values(rows):
    out = {i: Q(q) for i, q in rows}
    assert len(out) == len(rows)
    return out


def combine(a, b, sign=1):
    out = dict(a)
    for i, q in b.items():
        out[i] = out.get(i, Q(0)) + sign * q
    return out


def boundary(c, k, v):
    assert 0 < k < len(c['sizes'])
    out = {}
    for i, q in v.items():
        assert 0 <= i < c['sizes'][k]
        for upper, lower, _, coefficient, aspects in c['flags'][k]:
            if upper == i:
                out[lower] = out.get(lower, Q(0)) + (q * Q(coefficient) if aspects[0] else 0)
    return out


def strict(a, b, m):
    assert a['types'] == b['types'] and len(m['cells']) == len(a['sizes']) == len(m['flags'])
    assert len(b['sizes']) >= len(a['sizes'])
    for k, count in enumerate(a['sizes']):
        assert len(m['cells'][k]) == count and len(m['flags'][k]) == len(a['flags'][k])
        for i, j in enumerate(m['cells'][k]):
            assert 0 <= j < b['sizes'][k] and a['cell_types'][k][i] == b['cell_types'][k][j]
            if not k:
                continue
            source = [p for p, f in enumerate(a['flags'][k]) if f[0] == i]
            target = [p for p, f in enumerate(b['flags'][k]) if f[0] == j]
            assert sorted(m['flags'][k][p] for p in source) == target
            for p in source:
                f, g = a['flags'][k][p], b['flags'][k][m['flags'][k][p]]
                assert m['cells'][k-1][f[1]] == g[1] and Q(f[3]) == Q(g[3]) and f[4] == g[4]


def product_cells(a, b):
    return {(k, i, l, j) for k, n in enumerate(a['sizes']) for i in range(n)
            for l, m in enumerate(b['sizes']) for j in range(m)}


def routes(a, b, id):
    k, i, l, j = id
    out = []
    if k:
        for p, f in enumerate(a['flags'][k]):
            if f[0] == i:
                out.append(((k-1, f[1], l, j), 0, f[2], Q(f[3]), list(f[4]), p))
    if l:
        for p, f in enumerate(b['flags'][l]):
            if f[0] == j:
                aspects = list(f[4])
                aspects[1] = int(bool(aspects[1]) != bool(k % 2))
                out.append(((k, i, l-1, f[1]), 1, f[2], (-1)**k * Q(f[3]), aspects, p))
    return out


def push_product(s, v):
    out = {}
    for id, q in v.items():
        k = id[0] + id[2]
        i = s['h']['cells'][k][s['cell_map'][id]]
        out[i] = out.get(i, Q(0)) + q
    return out


def embed(v, k, end=None):
    return {(k, i, 1 if end is None else 0, 0 if end is None else end):
            q * ((-1)**k if end is None else 1) for i, q in v.items()}


def verify_source(s, counters):
    a, b, c, y = (s[k] for k in ['source', 'interval', 'carrier', 'target'])
    cells = {tuple(id): to for id, to in s['cells']}
    flags = {(tuple(id), side, occurrence): to for id, side, occurrence, to in s['flags']}
    assert len(cells) == len(s['cells']) and set(cells) == product_cells(a, b)
    assert len(flags) == len(s['flags'])
    assert len(c['sizes']) == len(a['sizes']) + len(b['sizes']) - 1
    types = {(left, right): out for left, right, out in s['type_interpretation']}
    assert len(types) == len(s['type_interpretation'])
    assert set(types) == set(itertools.product([None] + list(range(len(a['types']))), [None] + list(range(len(b['types'])))))
    for (left, right), out in types.items():
        if right is None:
            assert out == left
        else:
            assert out is not None and 0 <= out < len(c['types'])
    seen_flags = set()
    for k, n in enumerate(c['sizes']):
        assert sorted(to for id, to in cells.items() if id[0]+id[2] == k) == list(range(n))
    for id, i in cells.items():
        k, left, l, right = id
        if k+l:
            assert c['cell_types'][k+l][i] == types[a['cell_types'][k][left], b['cell_types'][l][right]]
        product_routes = routes(a, b, id)
        actual = [p for p, f in enumerate(c['flags'][k+l]) if f[0] == i]
        assert len(actual) == len(product_routes)
        for p, r in zip(actual, product_routes):
            lower, side, occurrence, coefficient, aspects, _ = r
            key = (id, side, occurrence)
            assert flags[key] == p and key not in seen_flags
            seen_flags.add(key)
            f = c['flags'][k+l][p]
            assert f[1] == cells[lower] and Q(f[3]) == coefficient and f[4] == aspects
            counters['retained_product_flags'] += 1
            counters['inactive_flags'] += not aspects[0]
            counters['active_zero_flags'] += bool(aspects[0]) and coefficient == 0
    assert seen_flags == set(flags)
    assert sum(len(row) for row in c['flags']) == len(flags)
    for first, end in [(s['first'], 0), (s['second'], 1)]:
        strict(a, y, first)
        for k, n in enumerate(a['sizes']):
            for i in range(n):
                assert first['cells'][k][i] == s['h']['cells'][k][cells[k, i, 0, end]]
                counters['endpoint_cells'] += 1
            for p, f in enumerate(a['flags'][k]):
                assert first['flags'][k][p] == s['h']['flags'][k][flags[((k, f[0], 0, end), 0, f[2])]]
                counters['endpoint_flags'] += 1
    strict(c, y, s['h'])
    for complex in [a, b, c, y]:
        for k in range(2, len(complex['sizes'])):
            for i in range(complex['sizes'][k]):
                assert not any(boundary(complex, k-1, boundary(complex, k, {i: Q(1)})).values())
            counters['complete_chain_families'] += 1
    # Full chain naturality is checked on every product basis cell, including
    # unused directions; it is separate from the per-input homotopy identity.
    s['cell_map'] = cells
    for id in cells:
        k = id[0]+id[2]
        if not k:
            continue
        source_boundary = {}
        for lower, _, _, coefficient, aspects, _ in routes(a, b, id):
            source_boundary[lower] = source_boundary.get(lower, Q(0)) + (coefficient if aspects[0] else 0)
        lhs = boundary(y, k, push_product(s, {id: Q(1)}))
        rhs = push_product(s, source_boundary)
        assert not any(combine(lhs, rhs, -1).values())
        counters['chain_naturality_directions'] += 1
    counters['carriers'] += 1
    counters['product_cells'] += len(cells)
    counters['noninjective_homotopies'] += any(len(set(row)) < len(row) for row in s['h']['cells'])


def verify_witness(s, w, counters):
    k, v = w['grade'], values(w['input'])
    a, y = s['source'], s['target']
    first, second = push_product(s, embed(v, k, 0)), push_product(s, embed(v, k, 1))
    h = push_product(s, embed(v, k))
    dh = boundary(y, k+1, h)
    d = boundary(a, k, v) if k else None
    hd = push_product(s, embed(d, k-1)) if k else {}
    difference = combine(second, first, -1)
    residual = combine(difference, combine(dh, hd), -1)
    assert (values(w['source_boundary']) if w['source_boundary'] is not None else None) == d
    for label, expected in [('first', first), ('second', second), ('h', h), ('dh', dh),
                            ('hd', hd), ('difference', difference), ('residual', residual)]:
        assert values(w[label]) == expected, (w['id'], k, label)
    assert not any(residual.values())
    counters['witnesses'] += 1
    counters['supported_zero_outputs'] += sum(q == 0 for q in h.values())
    counters['empty_inputs'] += not v


def main():
    process = subprocess.run([sys.argv[1], '--records'], check=True, capture_output=True, text=True)
    assert not process.stderr
    counters = dict(arithmetic='fractions.Fraction', carriers=0, product_cells=0, retained_product_flags=0,
                    inactive_flags=0, active_zero_flags=0, endpoint_cells=0, endpoint_flags=0,
                    complete_chain_families=0, chain_naturality_directions=0, noninjective_homotopies=0,
                    witnesses=0, supported_zero_outputs=0, empty_inputs=0)
    sources = {}
    for line in process.stdout.splitlines():
        record = json.loads(line)
        if record['kind'] == 'source':
            assert record['id'] not in sources
            sources[record['id']] = record
            verify_source(record, counters)
        else:
            assert record['kind'] == 'witness'
            verify_witness(sources[record['id']], record, counters)
    assert counters['carriers'] == 6 and counters['noninjective_homotopies'] == 1
    assert counters['inactive_flags'] and counters['active_zero_flags'] and counters['supported_zero_outputs']
    print(json.dumps(counters, sort_keys=True))


if __name__ == '__main__':
    main()
