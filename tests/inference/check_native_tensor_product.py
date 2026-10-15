"""Independent retained-occurrence and graded tensor actions using Fraction."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def field(raw):
    result = {}
    for p, i, q, j, value in raw:
        key = (p, i, q, j)
        assert key not in result
        result[key] = Q(value)
    return result


def matrix(raw):
    result = [[Q() for _ in range(raw['cols'])] for _ in range(raw['rows'])]
    seen = set()
    for i, j, value in raw['entries']:
        assert (i, j) not in seen and Q(value)
        seen.add((i, j))
        result[i][j] = Q(value)
    return result


def factor_boundary(source, k):
    sizes = source['sizes']
    result = [[Q() for _ in range(sizes[k])] for _ in range(sizes[k - 1])]
    keys = set()
    for grade, upper, lower, identity, exists, coefficient, aspects in source['occurrences']:
        assert (grade, identity) not in keys
        keys.add((grade, identity))
        assert exists == aspects[0] and len(aspects) == 3
        if grade == k and exists:
            result[lower][upper] += Q(coefficient)
    return result


def multiply(a, b):
    # Dimensions of these fixtures are nonempty; empty rectangular carriers
    # have separate C++ contracts.
    assert len(a[0]) == len(b)
    return [[sum((x * row[j] for x, row in zip(left, b)), Q())
             for j in range(len(b[0]))] for left in a]


def boundary(sources, values):
    out = {}
    for (p, i, q, j), value in values.items():
        for side, upper_grade, upper in [(0, p, i), (1, q, j)]:
            if not upper_grade:
                continue
            for grade, owner, lower, _, exists, coefficient, _ in sources[side]['occurrences']:
                if grade != upper_grade or owner != upper:
                    continue
                target = (p - 1, lower, q, j) if side == 0 else (p, i, q - 1, lower)
                out.setdefault(target, Q())
                if exists:
                    out[target] += value * Q(coefficient) * (-1 if side and p % 2 else 1)
    return out


def push(maps, values):
    out = {}
    for (p, i, q, j), value in values.items():
        left, right = maps[0][p], maps[1][q]
        for u in range(len(left)):
            if not left[u][i]:
                continue
            for v in range(len(right)):
                if right[v][j]:
                    target = (p, u, q, v)
                    out.setdefault(target, Q())
                    out[target] += value * left[u][i] * right[v][j]
    return out


done = subprocess.run([sys.argv[1], '--emit'], capture_output=True, text=True, check=True)
assert not done.stderr
records = [json.loads(line) for line in done.stdout.splitlines()]
counts = dict(supported_zero_inputs=0, chain_families=0, odd_right_signs=0,
              even_right_signs=0, retained_inactive_occurrences=0,
              active_zero_occurrences=0, repeated_participants=0,
              complete_factor_map_identities=0, supported_cancelled_outputs=0)
for item in records:
    sources, targets = item['sources'], item['targets']
    maps = [[matrix(m) for m in side] for side in item['maps']]
    raw = field(item['input'])
    assert all(p + q == item['grade'] and i < sources[0]['sizes'][p]
               and j < sources[1]['sizes'][q] for p, i, q, j in raw)
    expected = push(maps, raw)
    assert expected == field(item['output'])
    for side in range(2):
        for k in range(1, len(sources[side]['sizes'])):
            lhs = multiply(factor_boundary(targets[side], k), maps[side][k])
            rhs = multiply(maps[side][k - 1], factor_boundary(sources[side], k))
            assert lhs == rhs
            counts['complete_factor_map_identities'] += 1
    if item['grade']:
        before, after = boundary(sources, raw), boundary(targets, expected)
        assert before == field(item['source_boundary']) and after == field(item['target_boundary'])
        # Equality of realized values does not equate the two derived supports.
        pushed = push(maps, before)
        assert all(pushed.get(k, Q()) == after.get(k, Q()) for k in pushed.keys() | after.keys())
        if item['grade'] > 1:
            assert not any(boundary(sources, before).values())
            assert not any(boundary(targets, after).values())
            counts['chain_families'] += 1
    else:
        assert item['source_boundary'] is None and item['target_boundary'] is None
    for p, _, q, _ in raw:
        counts['odd_right_signs'] += q > 0 and p % 2 == 1
        counts['even_right_signs'] += q > 0 and p % 2 == 0
    counts['supported_zero_inputs'] += bool(raw) and not any(raw.values())
    counts['supported_cancelled_outputs'] += bool(expected) and not any(expected.values())
    for source in sources:
        occurrences = source['occurrences']
        counts['retained_inactive_occurrences'] += sum(not o[4] for o in occurrences)
        counts['active_zero_occurrences'] += sum(o[4] and not Q(o[5]) for o in occurrences)
        counts['repeated_participants'] += len(occurrences) > len({tuple(o[:3]) for o in occurrences})
assert len(records) == 576 and all(counts.values())
print(json.dumps(dict(native_cases=len(records), **counts, arithmetic='fractions.Fraction'), indent=2))
