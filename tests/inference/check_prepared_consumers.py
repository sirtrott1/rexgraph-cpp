#!/usr/bin/env python3
"""Independent Fraction boundary/adjoint/Hodge/model checks over emitted fields."""
import json
import subprocess
import sys
from fractions import Fraction as Q

B = []  # Reconstructed from retained scales, lattice values and existence per case.

def apply(a, x):
    return [sum((v*t for v, t in zip(row, x)), Q(0)) for row in a]

def adjoint(k, x, metrics):
    measured = [v*m for v, m in zip(x, metrics[k-1])]
    transposed = list(zip(*B[k]))
    return [v/m for v, m in zip(apply(transposed, measured), metrics[k])]

def down(k, x, metrics):
    return adjoint(k, apply(B[k], x), metrics) if k else [Q(0)]*len(x)

def up(k, x, metrics):
    return apply(B[k+1], adjoint(k+1, x, metrics)) if k < 2 else [Q(0)]*len(x)

raw = subprocess.check_output([sys.argv[1], '--oracle'], text=True)
cases = 0
for line in raw.splitlines():
    row = json.loads(line)
    k, kind = row['grade'], row['kind']
    source = row['source']
    B = [None]
    for grade in range(1, 3):
        matrix = [[Q(0)]*len(source[grade]['scales']) for _ in source[grade-1]['scales']]
        for occurrence in source[grade]['occurrences']:
            if occurrence['exists']:
                i, j = occurrence['lower'], occurrence['upper']
                matrix[i][j] += Q(source[grade-1]['scales'][i])*Q(occurrence['lattice'])/Q(source[grade]['scales'][j])
        B.append(matrix)
    assert all(sum(B[1][i][j]*B[2][j][h] for j in range(3)) == 0 for i in range(3) for h in range(2))
    x = list(map(Q, row['input']))
    metrics = [list(map(Q, m)) for m in row['metrics']]
    if kind == 0: expected = apply(B[k], x)
    elif kind == 1: expected = adjoint(k, x, metrics)
    elif kind == 2: expected = down(k, x, metrics)
    elif kind == 3: expected = up(k, x, metrics)
    elif kind == 4: expected = [a+b for a, b in zip(down(k, x, metrics), up(k, x, metrics))]
    elif kind == 99:
        d, u = down(k, x, metrics), up(k, x, metrics)
        expected = [Q(2,3) + 2*v - 3*a + 4*b + 5*(a+b) for v,a,b in zip(x,d,u)]
    else: raise AssertionError(kind)
    assert list(map(Q, row['output'])) == expected, row
    cases += 1
assert cases == 24*2*14, cases
print(json.dumps({'fraction_cases': cases, 'output_coordinates_checked': sum(len(json.loads(l)['output']) for l in raw.splitlines()), 'chain_coordinates_checked': cases*6}))
