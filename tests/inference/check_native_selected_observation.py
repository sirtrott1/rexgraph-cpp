"""Independent full Fraction solves and original-coordinate native readouts."""
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


done = subprocess.run([sys.argv[1], '--emit'], capture_output=True, text=True, check=True)
assert not done.stderr
records = [json.loads(line) for line in done.stdout.splitlines()]
empty = complete = proper = repeated = 0
for item in records:
    n, coordinates = item['n'], item['coordinates']
    a, s = [[Q(value) for value in row] for row in item['A']], [[Q(value) for value in row] for row in item['S']]
    rhs, actual = list(map(Q, item['rhs'])), list(map(Q, item['result']))
    assert len(rhs) == n and len(actual) == len(s) == len(coordinates)
    assert sorted(item['row_word']) == sorted(item['column_word']) == list(range(n))
    assert all(0 <= coordinate < n for coordinate in coordinates)
    assert all(len(row) == n for row in s)
    assert s == [[Q(j == coordinate) for j in range(n)] for coordinate in coordinates]
    full = solve(a, rhs)
    assert [sum(q * value for q, value in zip(row, full)) for row in a] == rhs
    assert actual == [sum(q * value for q, value in zip(row, full)) for row in s] == [full[i] for i in coordinates]
    assert item['full'] == (len(set(coordinates)) == n)
    if item['mode'] == 0:
        assert item['row_word'] == item['column_word'] == list(range(n))
    if item['full']:
        complete += 1
    else:
        proper += 1
    empty += not coordinates
    repeated += len(coordinates) != len(set(coordinates))
assert len(records) == 546 and empty == 114 and complete + proper == 546
print(json.dumps(dict(native_cases=len(records), empty_readouts=empty, complete_coverage=complete,
                      proper_subsets=proper, repeated_readouts=repeated, arithmetic='fractions.Fraction'), indent=2))
