#!/usr/bin/env python3
"""Independent dense Fraction elimination for the declared finite test systems."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def solve(matrix, rhs):
    rows = [list(row) + [value] for row, value in zip(matrix, rhs)]
    n = len(rows)
    for k in range(n):
        pivot = next(i for i in range(k, n) if rows[i][k])
        rows[k], rows[pivot] = rows[pivot], rows[k]
        scale = rows[k][k]
        rows[k] = [value / scale for value in rows[k]]
        for i in range(n):
            if i != k:
                ratio = rows[i][k]
                rows[i] = [x - ratio * y for x, y in zip(rows[i], rows[k])]
    return [row[-1] for row in rows]


cases = 0
for line in subprocess.check_output([sys.argv[1], '--emit'], text=True).splitlines():
    row = json.loads(line)
    matrix = [[Q(v) for v in r] for r in row['matrix']]
    rhs = list(map(Q, row['rhs']))
    expected = solve(matrix, rhs)
    transpose = [list(col) for col in zip(*matrix)]
    assert list(map(Q, row['full'])) == expected, row
    assert list(map(Q, row['transpose'])) == solve(transpose, rhs), row
    assert list(map(Q, row['selected'])) == [expected[i] for i in row['outputs']], row
    cases += 1
assert cases == 303, cases
print(f'sparse_selected_fraction_cases={cases}')
