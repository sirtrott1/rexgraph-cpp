"""Independent Fraction evaluation of the complete native update equations."""
from fractions import Fraction as F
import json
import subprocess
import sys


def matrix(rows, n, m):
    assert len(rows) == n and all(len(row) == m for row in rows)
    return [list(map(F, row)) for row in rows]


def solve(a, rhs):
    n = len(rhs)
    rows = [list(row) + [b] for row, b in zip(a, rhs)]
    for k in range(n):
        p = next(i for i in range(k, n) if rows[i][k])
        rows[k], rows[p] = rows[p], rows[k]
        divisor = rows[k][k]
        rows[k] = [q / divisor for q in rows[k]]
        for i in range(n):
            if i != k:
                ratio = rows[i][k]
                rows[i] = [q - ratio * v for q, v in zip(rows[i], rows[k])]
    return [row[-1] for row in rows]


done = subprocess.run([sys.argv[1], '--emit'], check=True, capture_output=True, text=True)
assert not done.stderr
records = [json.loads(line) for line in done.stdout.splitlines()]
assert len(records) == 216
noncommuting = 0
for case in records:
    n, rank = case['n'], case['rank']
    a = matrix(case['A'], n, n)
    u = matrix(case['U'], n, rank)
    c = matrix(case['C'], rank, rank)
    row = matrix(case['row'], rank, n)
    target = matrix(case['updated'], n, n)
    rhs, answer = list(map(F, case['rhs'])), list(map(F, case['answer']))
    assert len(rhs) == len(answer) == n
    expected = [[a[i][j] + sum(u[i][r] * c[r][s] * row[s][j]
                              for r in range(rank) for s in range(rank))
                 for j in range(n)] for i in range(n)]
    assert expected == target
    assert solve(expected, rhs) == answer
    assert [sum(target[i][j] * answer[j] for j in range(n)) for i in range(n)] == rhs
    # C is an invertibility hypothesis even when U or the ambient space is empty.
    for j in range(rank):
        basis = [F(i == j) for i in range(rank)]
        assert [sum(c[i][k] * solve(c, basis)[k] for k in range(rank)) for i in range(rank)] == basis
    if n and rank > 1:
        columns = [solve(a, [u[i][j] for i in range(n)]) for j in range(rank)]
        middle = [[sum(row[i][k] * columns[j][k] for k in range(n)) for j in range(rank)] for i in range(rank)]
        left = [[sum(c[i][k] * middle[k][j] for k in range(rank)) for j in range(rank)] for i in range(rank)]
        right = [[sum(middle[i][k] * c[k][j] for k in range(rank)) for j in range(rank)] for i in range(rank)]
        noncommuting += left != right
assert noncommuting > 0
print(f'native_update_fraction_cases={len(records)} noncommuting_cases={noncommuting}')
