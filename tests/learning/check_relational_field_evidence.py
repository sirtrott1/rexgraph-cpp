"""Independent Fraction elimination and complete correlated program responses."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def apply(matrix, x):
    return [sum((a*b for a, b in zip(row, x)), Q(0)) for row in matrix]


def product(a, b):
    return [[sum((x*y for x, y in zip(row, col)), Q(0)) for col in zip(*b)] for row in a]


def add(a, b):
    return [x+y for x, y in zip(a, b)]


def family(rows, target):
    n = 5
    reduced = [list(row)+[y] for row, y in zip(rows, target)]
    pivots = []
    for col in range(n):
        pivot = next((i for i in range(len(pivots), len(reduced)) if reduced[i][col]), None)
        if pivot is None:
            continue
        row = len(pivots)
        reduced[row], reduced[pivot] = reduced[pivot], reduced[row]
        scale = reduced[row][col]
        reduced[row] = [q/scale for q in reduced[row]]
        for i in range(len(reduced)):
            if i != row:
                scale = reduced[i][col]
                reduced[i] = [x-scale*y for x, y in zip(reduced[i], reduced[row])]
        pivots.append(col)
    assert all(any(row[:n]) or not row[-1] for row in reduced)
    free = [i for i in range(n) if i not in pivots]
    p = [Q(0)]*n
    k = [[Q(0)]*len(free) for _ in range(n)]
    for j, col in enumerate(free):
        k[col][j] = Q(1)
    for row, col in enumerate(pivots):
        p[col] = reduced[row][-1]
        for j, free_col in enumerate(free):
            k[col][j] = -reduced[row][free_col]
    return p, k


def design(x):
    a, b, c, d = x
    return [[a,b,0,0,0], [0,a,b,0,0], [d/2,0,c/2,0,0],
            [c/2,d/2,0,0,0], [0,c/2,d/2,0,0], [0]*5, [0]*5]


def original_flags(x):
    a, b, c, d = x
    return [[0,0], [-a,-b,-a-b], [-a-2*b,-b], [0]*3, [0]*2,
            [2*c+d,c+3*d], [0]*3, [-c+d,2*c-2*d]]


def base_field(x):
    a, b, c, d = x
    return [-a-2*b+2*c+d, -b+c+3*d, -a,-b,-a-b,-c+d,2*c-2*d]


def rational_matrix(rows):
    return [list(map(Q, row)) for row in rows]


truth = [Q(2), Q(-1), Q(3), Q(5), Q(7)]
first_design = design(list(map(Q, [1,2,0,0])))
second_design = design(list(map(Q, [0,0,1,3])))
p, k = family(first_design, apply(first_design, truth))
p2, k2 = family(first_design+second_design, apply(first_design, truth)+apply(second_design, truth))
count = 0
for line in subprocess.check_output([sys.argv[1], '--emit'], text=True).splitlines():
    record = json.loads(line)
    x = list(map(Q, record['input']))
    assert list(map(Q, record['parameter_particular'])) == p
    assert rational_matrix(record['parameter_kernel']) == k
    assert list(map(Q, record['learned_particular'])) == p2
    assert rational_matrix(record['learned_kernel']) == k2
    h = design(x)
    whole_p = add(base_field(x), apply(h, p))
    whole_k = product(h, k)
    app = rational_matrix(record['application'])
    assert list(map(Q, record['fields'][0]['particular'])) == x
    assert rational_matrix(record['fields'][0]['directions']) == [[Q(0)]*3 for _ in x]
    for field, expected_p, expected_k in [(record['fields'][1], whole_p, whole_k),
                                          (record['fields'][2], apply(app, whole_p), product(app, whole_k))]:
        assert list(map(Q, field['particular'])) == expected_p
        assert rational_matrix(field['directions']) == expected_k
    flags = original_flags(x)
    flags_base = [q for block in flags for q in block]
    flag_design = h[:2] + [[Q(0)]*5 for _ in range(12)] + h[2:5] + [[Q(0)]*5 for _ in range(2)]
    assert len(flag_design) == 19
    assert list(map(Q, record['flags']['particular'])) == add(flags_base, apply(flag_design, p))
    assert rational_matrix(record['flags']['directions']) == product(flag_design, k)
    assert list(map(Q, record['materialized'])) == add(base_field(x), apply(h, truth))
    count += 1
assert count == 40
print(f'relational_field_evidence_fraction_cases={count}')
