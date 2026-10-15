#!/usr/bin/env python3
"""Dense Fraction verification of full update equations and every delta term."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def matrix(rows):
    return [[Q(x) for x in row] for row in rows]


def transpose(a):
    return list(map(list, zip(*a)))


def mul(a, b):
    return [[sum(x*y for x, y in zip(row, col)) for col in transpose(b)] for row in a]


def add(a, b, scale=Q(1)):
    return [[x+scale*y for x, y in zip(row, other)] for row, other in zip(a, b)]


def apply(a, x):
    return [sum(q*v for q, v in zip(row, x)) for row in a]


def sub(x, y):
    return [a-b for a, b in zip(x, y)]


def shifted(a, q):
    return [[Q(i == j)+q*x for j, x in enumerate(row)] for i, row in enumerate(a)]


def solve(a, rhs):
    rows = [row.copy()+[v] for row, v in zip(a, rhs)]
    for k in range(len(rows)):
        pivot = next(i for i in range(k, len(rows)) if rows[i][k])
        rows[k], rows[pivot] = rows[pivot], rows[k]
        value = rows[k][k]
        rows[k] = [x/value for x in rows[k]]
        for i in range(len(rows)):
            if i != k:
                value = rows[i][k]
                rows[i] = [x-value*y for x, y in zip(rows[i], rows[k])]
    return [row[-1] for row in rows]


counts = dict(woodbury=0, green=0)
for line in subprocess.check_output([sys.argv[1], '--emit'], text=True).splitlines():
    row = json.loads(line)
    kind = row['kind']
    if kind == 'woodbury':
        a, u, c, v = [matrix(row[key]) for key in ['A', 'U', 'C', 'V']]
        updated = add(a, mul(mul(u, c), transpose(v)))
        rhs = list(map(Q, row['rhs']))
        answer = list(map(Q, row['answer']))
        assert answer == solve(updated, rhs) and apply(updated, answer) == rhs
    else:
        old, new, j, old_i, new_i, k, old_a, new_a, ka = [matrix(row[key]) for key in
            ['old_L', 'new_L', 'J', 'old_I', 'new_I', 'K', 'old_A', 'new_A', 'KA']]
        u, up = [list(map(Q, row[key])) for key in ['u', 'up']]
        q = Q(row['lambda'])
        s0, s1 = shifted(old, q), shifted(new, q)
        g, gp = solve(s0, apply(old_i, u)), solve(s1, apply(new_i, up))
        ei, el = add(mul(new_i, k), mul(j, old_i), -1), add(mul(new, j), mul(j, old), -1)
        real = solve(s1, apply(ei, u))
        innovation = solve(s1, apply(new_i, sub(up, apply(k, u))))
        operator = [-q*x for x in solve(s1, apply(el, g))]
        aggregate = [sum(values) for values in zip(real, innovation, operator)]
        assert aggregate == sub(gp, apply(j, g))
        for name, expected in [('old_response', g), ('new_response', gp), ('realization', real),
                               ('innovation', innovation), ('operator', operator), ('aggregate', aggregate)]:
            assert list(map(Q, row[name])) == expected, name
        ea = add(mul(new_a, j), mul(ka, old_a), -1)
        assert matrix(row['EI']) == ei and matrix(row['EL']) == el and matrix(row['EA']) == ea
        observed = sub(apply(new_a, gp), apply(ka, apply(old_a, g)))
        assert list(map(Q, row['observed'])) == observed
    counts[kind] += 1
assert counts == dict(woodbury=108, green=180), counts
print('prepared_update_fraction_cases=288 woodbury=108 green=180')
