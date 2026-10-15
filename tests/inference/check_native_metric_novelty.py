"""Independent Fraction elimination, complete metric complements and cause tensors."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def solve(a, rhs):
    rows = [row[:] + [value] for row, value in zip(a, rhs)]
    for j in range(len(rhs)):
        pivot = next(i for i in range(j, len(rhs)) if rows[i][j])
        rows[j], rows[pivot] = rows[pivot], rows[j]
        scale = rows[j][j]
        rows[j] = [value / scale for value in rows[j]]
        for i in range(len(rhs)):
            if i != j:
                scale = rows[i][j]
                rows[i] = [value - scale * coefficient for value, coefficient in zip(rows[i], rows[j])]
    return [row[-1] for row in rows]


def apply(a, x):
    assert all(len(row) == len(x) for row in a)
    return [sum((v * w for v, w in zip(row, x)), Q()) for row in a]


def dot(a, b):
    assert len(a) == len(b)
    return sum((x * y for x, y in zip(a, b)), Q())


def matrix(raw):
    return [list(map(Q, row)) for row in raw]


done = subprocess.run([sys.argv[1], '--emit'], capture_output=True, text=True, check=True)
assert not done.stderr
records = [json.loads(line) for line in done.stdout.splitlines()]
counts = dict(empty_old=0, empty_new=0, rectangular=0, nonidentity_metrics=0,
              nonorthogonal_causes=0, repeated_channels=0, cancelling_channels=0, zero_channels=0)
for item in records:
    n, m = item['n'], item['m']
    j, metric = matrix(item['J']), matrix(item['M'])
    old, new = list(map(Q, item['old'])), list(map(Q, item['new']))
    assert len(old) == n and len(new) == len(j) == len(metric) == m
    assert all(len(row) == n for row in j) and all(len(row) == m for row in metric)
    columns = [[row[k] for row in j] for k in range(n)]
    # Independent full-column Gram construction and dense elimination in this
    # finite verifier; production retains its original sparse prepared factor.
    gram = [[dot(a, apply(metric, b)) for b in columns] for a in columns]
    coords = solve(gram, [dot(col, apply(metric, new)) for col in columns])
    change = [a - b for a, b in zip(coords, old)]
    persistent = apply(j, change)
    complement = [a - b for a, b in zip(new, apply(j, coords))]
    innovation = [a - b for a, b in zip(new, apply(j, old))]
    assert change == list(map(Q, item['coordinates']))
    assert persistent == list(map(Q, item['persistent']))
    assert complement == list(map(Q, item['complement']))
    assert innovation == list(map(Q, item['innovation']))
    assert [a + b for a, b in zip(persistent, complement)] == innovation
    assert all(dot(col, apply(metric, complement)) == 0 for col in columns)
    parts = [persistent, complement]
    novelty_gram = [[dot(a, apply(metric, b)) for b in parts] for a in parts]
    assert novelty_gram == matrix(item['novelty_gram']) and novelty_gram[0][1] == novelty_gram[1][0] == 0
    quadrance = dot(innovation, apply(metric, innovation))
    assert quadrance == Q(item['quadrance']) == sum(map(sum, novelty_gram))
    causes = matrix(item['causes'])
    assert len(causes) == 5 and all(len(v) == m for v in causes)
    assert [sum((v[k] for v in causes), Q()) for k in range(m)] == innovation
    pairs = [[dot(a, apply(metric, b)) for b in causes] for a in causes]
    assert pairs == matrix(item['cause_gram']) and sum(map(sum, pairs)) == quadrance
    counts['empty_old'] += n == 0
    counts['empty_new'] += m == 0
    counts['rectangular'] += n != m
    counts['nonidentity_metrics'] += metric != [[Q(i == k) for k in range(m)] for i in range(m)]
    counts['nonorthogonal_causes'] += any(pairs[i][k] for i in range(5) for k in range(5) if i != k)
    counts['repeated_channels'] += causes[0] == causes[1]
    counts['cancelling_channels'] += causes[2] == [-v for v in causes[0]]
    counts['zero_channels'] += not any(causes[4])
assert len(records) == 324 and all(counts.values())
print(json.dumps(dict(native_cases=len(records), **counts, arithmetic='fractions.Fraction'), indent=2))
