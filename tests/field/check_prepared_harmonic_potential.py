#!/usr/bin/env python3
"""Independent small dense Fraction reference; production retains original maps."""
import json
import subprocess
import sys
from fractions import Fraction as Q


def zero(rows, cols):
    return [[Q(0) for _ in range(cols)] for _ in range(rows)]


def matrix(record):
    rows, cols = record['rows'], record['cols']
    values = [[Q(v) for v in row] for row in record['values']]
    assert len(values) == rows and all(len(row) == cols for row in values)
    return values


def transpose(a, cols):
    return [[a[i][j] for i in range(len(a))] for j in range(cols)]


def multiply(a, b, cols):
    return [[sum((x * b[k][j] for k, x in enumerate(row)), Q(0))
             for j in range(cols)] for row in a]


def apply(a, x):
    return [sum((v * t for v, t in zip(row, x)), Q(0)) for row in a]


def reduced(a, cols):
    a = [list(row) for row in a]
    pivots = []
    for j in range(cols):
        k = len(pivots)
        p = next((i for i in range(k, len(a)) if a[i][j]), None)
        if p is None:
            continue
        a[k], a[p] = a[p], a[k]
        scale = a[k][j]
        a[k] = [v / scale for v in a[k]]
        for i in range(len(a)):
            if i != k:
                factor = a[i][j]
                a[i] = [v - factor * w for v, w in zip(a[i], a[k])]
        pivots.append(j)
    return a, pivots


def solve(a, rhs):
    n = len(rhs)
    r, p = reduced([row + [v] for row, v in zip(a, rhs)], n)
    assert p == list(range(n)), 'singular independent reference'
    return [row[-1] for row in r]


def inverse(a):
    n = len(a)
    return transpose([solve(a, [Q(i == j) for i in range(n)]) for j in range(n)], n)


def kernel(a, n):
    r, p = reduced(a, n)
    free = [j for j in range(n) if j not in p]
    f = zero(n, len(free))
    for col, j in enumerate(free):
        f[j][col] = Q(1)
        for i, pivot in enumerate(p):
            f[pivot][col] = -r[i][j]
    return f, len(free)


cases = 0
for line in subprocess.check_output([sys.argv[1], '--emit'], text=True).splitlines():
    row = json.loads(line)
    k = row['grade']
    metrics = [matrix(m) for m in row['metrics']]
    boundaries = [None] + [matrix(b) for b in row['boundaries']]
    x = list(map(Q, row['input']))
    n = len(x)
    lower = boundaries[k] if k else zero(0, n)
    down = zero(n, n)
    if k:
        bt = transpose(lower, n)
        down = multiply(inverse(metrics[k]), multiply(bt, multiply(metrics[k-1], lower, n), n), n)
    up = zero(n, n)
    if k + 1 < len(metrics):
        c = boundaries[k+1]
        u = row['metrics'][k+1]['rows']
        up = multiply(c, multiply(inverse(metrics[k+1]), multiply(transpose(c, u), metrics[k], n), n), n)
    l = [[a + b for a, b in zip(ra, rb)] for ra, rb in zip(down, up)]
    h, count = kernel(l, n)
    ph = zero(n, n)
    if count:
        ht = transpose(h, count)
        dual = multiply(ht, metrics[k], n)
        gram = multiply(dual, h, count)
        ph = multiply(h, multiply(inverse(gram), dual, n), n)
    completed = [[a + b for a, b in zip(ra, rb)] for ra, rb in zip(l, ph)]
    expected = [a - b for a, b in zip(solve(completed, x), apply(ph, x))]
    assert list(map(Q, row['inverse'])) == expected, row
    assert apply(l, expected) == [a - b for a, b in zip(x, apply(ph, x))]
    assert apply(ph, expected) == [Q(0)] * n
    potential = apply(lower, expected) if k else []
    assert list(map(Q, row['potential'])) == potential, row
    if k:
        low_n = len(lower)
        adjoint = multiply(inverse(metrics[k]), multiply(transpose(lower, n), metrics[k-1], low_n), low_n)
        assert apply(adjoint, potential) == apply(down, expected)
        z, z_count = kernel(adjoint, low_n)
        assert apply(transpose(z, z_count), apply(metrics[k-1], potential)) == [Q(0)] * z_count
    cases += 1
assert cases == 120, cases
print(json.dumps({'independent_fraction_inverse_and_full_gauge_cases': cases}))
