"""Independent exact full solves and complete rational accession observations."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def solve(a, rhs):
    n = len(rhs)
    assert len(a) == n and all(len(row) == n for row in a)
    rows = [row[:] + [value] for row, value in zip(a, rhs)]
    for j in range(n):
        pivot = next(i for i in range(j, n) if rows[i][j])
        rows[j], rows[pivot] = rows[pivot], rows[j]
        scale = rows[j][j]
        rows[j] = [value / scale for value in rows[j]]
        for i in range(n):
            if i != j:
                scale = rows[i][j]
                rows[i] = [value - scale * coefficient for value, coefficient in zip(rows[i], rows[j])]
    return [row[-1] for row in rows]


def proportional(a, b):
    index = next((i for i, value in enumerate(a) if value), None)
    return index is not None and b[index] and b != a and all(y == b[index] / a[index] * x for x, y in zip(a, b))


done = subprocess.run([sys.argv[1], '--emit'], capture_output=True, text=True, check=True)
assert not done.stderr
records = [json.loads(line) for line in done.stdout.splitlines()]
counts = dict(empty_inputs=0, empty_outputs=0, zero_maps=0, repeated_rows=0,
              proportional_rows=0, primal_actions=0, transpose_actions=0, rectangular_maps=0)
for item in records:
    n, q = item['n'], item['q']
    a, s = [[Q(value) for value in row] for row in item['A']], [[Q(value) for value in row] for row in item['S']]
    rhs, actual = list(map(Q, item['rhs'])), list(map(Q, item['result']))
    assert len(rhs) == n and len(actual) == len(s) == q and all(len(row) == n for row in s)
    full = solve(a, rhs)
    assert [sum(value * x for value, x in zip(row, full)) for row in a] == rhs
    assert actual == [sum(value * x for value, x in zip(row, full)) for row in s]
    unique, word = [], []
    for row in s:
        if row not in unique:
            unique.append(row)
        word.append(unique.index(row))
    nonzero = sum(any(row) for row in unique)
    assert item['reuse_word'] == word and item['distinct_rows'] == len(unique)
    assert item['full'] == (nonzero >= n)
    assert item['certified_nonzero_rows'] == (0 if item['full'] else nonzero)
    # Verify every independent transpose certificate with this separate solver.
    if not item['full']:
        transposed = [list(column) for column in zip(*a)]
        for row in unique:
            if any(row):
                dual = solve(transposed, row)
                assert [sum(value * x for value, x in zip(column, dual)) for column in transposed] == row
                assert sum(value * x for value, x in zip(dual, rhs)) == sum(value * x for value, x in zip(row, full))
    counts['empty_inputs'] += n == 0
    counts['empty_outputs'] += q == 0
    counts['zero_maps'] += not any(any(row) for row in s)
    counts['repeated_rows'] += len(unique) != q
    counts['proportional_rows'] += any(proportional(row, other) for i, row in enumerate(unique) for other in unique[i+1:])
    counts['primal_actions' if item['full'] else 'transpose_actions'] += 1
    counts['rectangular_maps'] += n != q
assert len(records) == 1080 and counts['empty_inputs'] == 180 and counts['empty_outputs'] == 216
assert all(value for value in counts.values())
print(json.dumps(dict(native_cases=len(records), **counts, arithmetic='fractions.Fraction'), indent=2))
